# Java Flag Generator

Generates a Java class with typed accessors for the features that a C++
feature list exposes to Java through `kFeaturesExposedToJava`.

For each `&namespace::kFoo` in the array, the generated class contains:

```java
/** {@code ns::kFoo} */
public static final class Foo {
    public static final String NAME = "FooFeatureName";

    public static boolean isEnabled() {
        return isFeatureEnabled(NAME);
    }
}
```

`NAME` is the feature name string from the feature's definition
(`BASE_FEATURE(kFoo, "FooFeatureName", ...)`), not a name derived from the
identifier.

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

* `sources`: C++ files that define `kFeaturesExposedToJava`.
* `class_name`: Fully-qualified name of the generated class.
* `feature_map_class_name` (optional): `FeatureMap` subclass used by all
  sources. By default, each source's map is derived from its
  `JNI_<Name>_GetNativeMap` function and assumed to be in the package of
  `class_name`.
* `deps` (optional): Must include the targets that generate any generated
  `FEATURE_DEFINITION_FILE`s.

With a single `FeatureMap`, the generated helper is `isFeatureEnabled()`. With
several (multiple sources backed by different maps), there is one helper per
map, named `isFeatureEnabledIn<FeatureMapName>()`.

## Finding feature definitions

For each feature in kFeaturesExposedToJava, the generator looks for feature
definitions in:

1. The source itself.
2. For each quoted `#include "path/foo.h"`, the file `path/foo.cc`. The path is
   tried relative to the source root, then relative to the including file.
3. Files listed in `FEATURE_DEFINITION_FILE` directives.

If a feature's definition can't be found, the build fails. Fix it by adding a
directive to the source naming the file that defines the feature:

```c++
// FEATURE_DEFINITION_FILE: //third_party/blink/common/features.cc
```

Directive paths are looked up in the source tree first, then in
`$root_gen_dir`. For a generated file, add the target that generates it to
`deps`.

Directives that are not needed (the file is already found automatically, or no
feature uses it) are errors, so the list stays accurate.

### What counts as a definition

Any ALL_CAPS macro call whose name contains `FEATURE` (but not `DECLARE` or
`PARAM`) and whose first argument is a `kCamelCase` identifier, e.g.
`BASE_FEATURE(...)` or `BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(...)`.
For `BASE_FEATURE` and `BASE_RUNTIME_MUTABLE_FEATURE`, the feature name is the
second argument when it is a string literal; otherwise the feature name is
derived from the identifier (`kFoo` -> `"Foo"`).
