---
name: general/prefer-buildflag-over-raw-preprocessor-defines
rubric: readability
severity: should_fix
---

**Rule.** Use the BUILDFLAG(...) macro and GN buildflag_header targets for build
configuration and conditional compilation rather than custom preprocessor
#defines or #ifdef checks. Include the corresponding generated buildflags header
and guard conditional code with #if BUILDFLAG(FLAG_NAME).

**Why.** Raw preprocessor #define and #ifdef guards silently evaluate to false
if the defining header is omitted or misspelled. The BUILDFLAG system produces a
compile-time error whenever an unincluded or undeclared flag is referenced,
preventing silent configuration bugs.

## Examples

Bad:

```cpp
#include "build/config/android/native_build_config_test.h"

#if NATIVE_IS_FOR_TEST
  base::test::AllowCheckIsTestForTesting();
#endif
```

Good:

```cpp
#include "base/android/test_buildflags.h"

#if BUILDFLAG(NATIVE_IS_FOR_TEST)
  base::test::AllowCheckIsTestForTesting();
#endif
```
