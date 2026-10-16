---
name: general/place-explanatory-comments-directly-above-declaration
rubric: readability
severity: nit
---

**Rule.** Place explanatory comments for a variable, constant, symbol, macro, or
header constraint (such as include-ordering requirements for generated *\_jni.h
files) immediately above the specific construct or #include directive they
document, rather than placing them above intervening declarations, newly added
includes, or internal implementation helper macros (such as _INTERNAL_*). Remove
include-ordering comments if they become obsolete.

**Why.** Separating an explanatory comment from the construct, public macro, or
header directive it documents with intervening definitions creates ambiguity
about which item the comment describes, misleadingly implies the documentation
applies to internal helpers or intervening code, and causes comments to rot over
time.

## Examples

Bad:

```cpp
// Variant 1: Comment separated from variable declaration by intervening constant
// The following thread-local variable is used to synchronize the MessagePump.
constexpr int kDefaultInitialNestingLevel = 1;
thread_local int initial_nesting_level = kDefaultInitialNestingLevel;

// Variant 2: Include-ordering comment detached by intervening includes
// Must come after all headers that specialize FromJniType() / ToJniType().
#include <jni.h>

#include "base/android/jni_android.h"
#include "base/sys_utils_jni/SysUtils_jni.h"

// Variant 3: Public macro documentation placed above internal helper macro
// Same as BASE_FEATURE_PARAM() but used for enum type parameters.
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS(T, feature_name, ...) ...
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_5_ARGS(...) ...
#define BASE_FEATURE_ENUM_PARAM(...) ...
```

Good:

```cpp
// Variant 1: Comment placed directly above the target variable declaration
constexpr int kDefaultInitialNestingLevel = 1;
// The following thread-local variable is used to synchronize the MessagePump.
thread_local int initial_nesting_level = kDefaultInitialNestingLevel;

// Variant 2: Clean includes colocated with target header
#include <jni.h>

#include "base/android/jni_android.h"
#include "base/process/process_metrics.h"
#include "base/sys_utils_jni/SysUtils_jni.h"

// Variant 3: Documentation placed directly above public macro definition
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_6_ARGS(T, feature_name, ...) ...
#define BASE_FEATURE_ENUM_PARAM_INTERNAL_5_ARGS(...) ...
// Same as BASE_FEATURE_PARAM() but used for enum type parameters.
#define BASE_FEATURE_ENUM_PARAM(...) ...
```
