---
name: base/use-memory-tool-replaces-allocator-buildflag
rubric: correctness
severity: should_fix
---

**Rule.** When conditionally compiling code for builds where a memory tool or
sanitizer replaces the allocator, check
PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR) (or
defined(MEMORY_TOOL_REPLACES_ALLOCATOR)); do not use undefined macros such as
MEMORY_TOOL_REPLACES_MALLOC.

**Why.** MEMORY_TOOL_REPLACES_MALLOC is not defined in Chromium or
PartitionAlloc. Using undefined macro names in preprocessor conditionals
silently evaluates to false, causing required headers or allocator overrides to
be omitted in memory-tool builds (such as ASan).

## Examples

Bad:

```cpp
#if defined(MEMORY_TOOL_REPLACES_MALLOC)
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif
```

Good:

```cpp
#if PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#endif
```
