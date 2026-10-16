---
name: general/match-definition-order-in-cc-to-header-declaration-order
rubric: readability
severity: should_fix
---

**Rule.** In C++ implementation files (.cc, .cpp, .mm), keep function and method
definitions in the same relative order as their corresponding declarations in
the header (.h) file. When reordering or relocating declarations in a header,
update the implementation file so definitions maintain matching order.

**Why.** Maintaining identical declaration and definition ordering between
headers and implementation files makes navigating between declarations and
implementations predictable, assists code review, and prevents definitions from
being arbitrarily misplaced across the file.

## Examples

Bad:

```cpp
// histogram_base.h
class BASE_EXPORT HistogramBase { ... };
BASE_EXPORT HistogramBase* DeserializeHistogramInfo(...);

// histogram_base.cc: definition precedes HistogramBase methods despite being declared after
HistogramBase* DeserializeHistogramInfo(...) { ... }
HistogramBase::HistogramBase(...) { ... }
```

Good:

```cpp
// histogram_base.h
class BASE_EXPORT HistogramBase { ... };
BASE_EXPORT HistogramBase* DeserializeHistogramInfo(...);

// histogram_base.cc: definition order matches declaration order in header
HistogramBase::HistogramBase(...) { ... }
HistogramBase* DeserializeHistogramInfo(...) { ... }
```
