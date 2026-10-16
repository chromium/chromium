---
name: general/avoid-inline-purpose-comments-on-includes
rubric: readability
severity: nit
---

**Rule.** Do not add inline comments to #include directives explaining which
symbols, types, or sub-headers they are included for (e.g., '// for Foo' or '//
for \<sub_header.h>'). Keep include directives clean and unannotated unless
required for tool directives like '// nogncheck'.

**Why.** Comments explaining why a header is included or which symbols it
provides rot quickly as the file evolves and other code references the header,
creating misleading documentation and unnecessary maintenance churn.

## Examples

Bad:

```cpp
// Bad: Annotating includes with specific symbols or sub-headers
#include <CoreServices/CoreServices.h>  // for <AE/AppleEvents.h>
#include "base/strings/string_util.h"  // for JoinString
```

Good:

```cpp
// Good: Clean include directives without symbol annotations
#include <CoreServices/CoreServices.h>
#include "base/strings/string_util.h"
```
