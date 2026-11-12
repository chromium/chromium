# Java Flag Generator

Generates a Java class with typed accessors for the features and params that a
C++ feature list exposes to Java through `kFeaturesExposedToJava` and
`k<Type>ParamsExposedToJava`.

## Features

For each `&namespace::kFooFeature` in `kFeaturesExposedToJava`, the generated
class contains:

```java
/** {@code namespace::kFooFeature} */
public static final class FooFeature {
    public static final String NAME = "FooFeatureName";

    public static boolean isEnabled() {
        return isFeatureEnabled(NAME);
    }
}
```

`NAME` is the feature name string from the feature's definition
(`BASE_FEATURE(kFooFeature, "FooFeatureName", ...)`), not a name derived from
the identifier.

## Params

Params are exposed to Java by listing them in one array per param type:
`kBooleanParamsExposedToJava`, `kIntParamsExposedToJava`,
`kDoubleParamsExposedToJava` and `kStringParamsExposedToJava`. These arrays are
optional. For each `&namespace::kFooParam` in them, the generated class
contains:

```java
/** {@code namespace::kFooParam} */
public static final class FooParam {
    public static final String NAME = "foo_param";

    public static int getValue() {
        return getParamValueAsInt(FooFeature.NAME, NAME);
    }
}
```

`NAME` is the param name string from the param's definition
(`BASE_FEATURE_PARAM(int, kFooParam, &kFooFeature, "foo_param", 0)`).
`FooFeature.NAME` is the feature name of the param's feature (`kFooFeature`),
which is usually referenced through the feature's nested class. The type
returned by `getValue()` (`boolean`, `int`, `double` or `String`) depends on the
array listing the param.

## Usage

```gn
import("//base/android/java_flag_generator/java_flag_generator.gni")

java_flag_generator("java_flags") {
  sources = [ "android/chrome_feature_list.cc" ]
  class_name = "org.chromium.chrome.browser.flags.ChromeFeatures"
}

android_library("java") {
  srcjar_deps = [ ":java_flags" ]
  ...
}
```

List the target in the `srcjar_deps` of exactly one `android_library()`;
others should depend on that library.

### Variables

* `sources`: C++ files that define `kFeaturesExposedToJava`, and optionally
  `k<Type>ParamsExposedToJava`.
* `class_name`: Fully-qualified name of the generated class.
* `feature_map_class_name` (optional): `FeatureMap` subclass used by all
  sources. By default, each source's map is derived from its
  `JNI_<Name>_GetNativeMap` function and assumed to be in the package of
  `class_name`.
* `deps` (optional): Must include the targets that generate any generated
  `FEATURE_DEFINITION_FILE`s.

## Finding feature and param definitions

For each feature in `kFeaturesExposedToJava` and each param in the
`k<Type>ParamsExposedToJava` arrays, the generator looks for definitions in:

1. The source itself.
2. For each quoted `#include "path/foo.h"`, the file `path/foo.cc`. The path is
   tried relative to the source root, then relative to the including file.
3. Files listed in `FEATURE_DEFINITION_FILE` directives.

The feature of each param (`&kFooFeature` in
`BASE_FEATURE_PARAM(int, kFooParam, &kFooFeature, 0)`) is looked up in the same
files, relative to the namespace of the param's definition.

If a definition can't be found, the build fails. Fix it by adding a directive
to the source naming the file that defines the feature or param:

```c++
// FEATURE_DEFINITION_FILE: //third_party/blink/common/features.cc
```

Directive paths are looked up in the source tree first, then in
`$root_gen_dir`. For a generated file, add the target that generates it to
`deps`.

Directives that are not needed (the file is already found automatically, or no
feature or param uses it) are errors, so the list stays accurate.

### What counts as a definition

For features: any ALL_CAPS macro call whose name contains `FEATURE` (but not
`DECLARE` or `PARAM`) and whose first argument is a `kCamelCase` identifier,
e.g. `BASE_FEATURE(...)` or `BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(...)`.
For `BASE_FEATURE` and `BASE_RUNTIME_MUTABLE_FEATURE`, the feature name is the
second argument when it is a string literal; otherwise the feature name is
derived from the identifier (`kFooFeature` -> `"FooFeature"`).

For params: any `BASE_FEATURE_PARAM(T, kFooParam, &kFooFeature, ...)` call.
With 5 arguments, the param name is the fourth argument, which must be a string
literal. With 4 arguments, the param name is derived from the identifier
(`kFooParam` -> `"FooParam"`). Declarations (`BASE_DECLARE_FEATURE_PARAM`) are
not definitions.

## Preprocessor conditionals

The generator only reads the code that is compiled in the current build, as far
as the conditions of `#if` and `#elif` can tell. It evaluates them with these
values:

* The OS flags of `//build/build_config.h` have their Android values:
  `BUILDFLAG(IS_ANDROID)` and `BUILDFLAG(IS_POSIX)` are true, and the others,
  such as `BUILDFLAG(IS_WIN)`, are false.
* `BUILDFLAG(IS_DESKTOP_ANDROID)` is the value of the `is_desktop_android` GN
  arg, so the generated class can differ between desktop and other Android
  builds.
* Anything else, such as `BUILDFLAG(FOO)`, `defined(FOO)` or `DCHECK_IS_ON()`,
  is unknown. Known values only decide a condition if the unknown parts can't
  change the result: `BUILDFLAG(IS_WIN) && defined(FOO)` is false, and
  `BUILDFLAG(IS_ANDROID) || defined(FOO)` is true. Otherwise, a condition with
  unknown parts is unknown as a whole, e.g.
  `!BUILDFLAG(IS_ANDROID) || BUILDFLAG(FOO)`. Conditions that are unknown as a
  whole are assumed to be true, even when negated, and so are the conditions of
  `#ifdef` and `#ifndef`. For example, the first branch of
  `#if !defined(FOO) ... #else ... #endif` is used.

Code in branches that are not compiled is ignored, including `#include`s and
`FEATURE_DEFINITION_FILE` comments. If a `FEATURE_DEFINITION_FILE` is only
needed in some builds, put it in the same `#if` block as the entries that need
it. `#if` blocks are supported inside `kFeaturesExposedToJava` and
`k<Type>ParamsExposedToJava`, but other preprocessor directives are not.

## Changing the generator

* Golden tests that run the generator end to end live in
  `test/integration_tests.py`. To update the expected output in `test/golden/`,
  run:
  ```sh
  REBASELINE=1 base/android/java_flag_generator/test/integration_tests.py
  ```
* The inputs of each of these tests live in `test/data/<test name>/`, which
  becomes the source root (`//`) of the test. Files generated in
  `$root_gen_dir` go under `out/Test/gen/` in that directory. Input files end
  with an extra `.in` (e.g. `foo_features.cc.in`) so that presubmit checks and
  formatters for C++ ignore them.
* Unit tests for the C++ lexer and parser live in `test/parse_test.py`.
* Both run as part of the presubmit checks of this directory (see
  `PRESUBMIT.py`).
