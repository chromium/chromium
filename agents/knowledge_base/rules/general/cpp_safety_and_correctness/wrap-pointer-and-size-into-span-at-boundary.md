---
name: general/wrap-pointer-and-size-into-span-at-boundary
rubric: safety
severity: nit
---

**Rule.** When receiving raw C-style pointer-and-size pairs (such as main(int
argc, char\*\* argv) or external C API buffers), wrap them into a base::span
once at the entry boundary using UNSAFE_BUFFERS(...) with a dedicated // SAFETY:
comment. Avoid repeating UNSAFE_BUFFERS(...) across multiple individual indexing
or pointer arithmetic operations throughout the function body.

**Why.** Wrapping pointer-and-size pairs into a base::span once at the boundary
confines the unsafe operation to a single audited conversion. Subsequent
accesses benefit from safe, bounds-checked span indexing without duplicating
UNSAFE_BUFFERS annotations and boilerplate safety comments throughout the
caller.

## Examples

Bad:

```cpp
// SAFETY: Standard C++ main contract.
unordered_map<string, unordered_map<string, vector<double>>> before =
    ReadFile(UNSAFE_BUFFERS(argv[1]));
// SAFETY: Standard C++ main contract.
unordered_map<string, unordered_map<string, vector<double>>> after =
    ReadFile(UNSAFE_BUFFERS(argv[2]));
```

Good:

```cpp
// SAFETY: Standard C++ main contract guarantees argv has argc valid elements.
auto args = UNSAFE_BUFFERS(base::span(argv, static_cast<size_t>(argc)));
unordered_map<string, unordered_map<string, vector<double>>> before =
    ReadFile(args[1]);
unordered_map<string, unordered_map<string, vector<double>>> after =
    ReadFile(args[2]);
```
