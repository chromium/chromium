---
name: general/use-decltype-auto-for-generic-range-accessor-return-types
rubric: readability
severity: nit
---

**Rule.** In generic forwarding functions, wrapper templates, or container and
range accessors, prefer using 'decltype(auto)' as the return type instead of
manually synthesizing reference types with traits (such as
'std::ranges::range_value_t<Range>&' or 'typename Container::reference'). Ensure
the return expression directly forwards or indexes the underlying element.

**Why.** Plain 'auto' decays references to value copies, while manually
constructed traits like 'range_value_t<Range>&' unnecessarily restrict returns
to non-const lvalue references, breaking const ranges, rvalues, and proxy
reference types (such as 'std::vector<bool>::reference'). 'decltype(auto)'
perfectly preserves the exact value category, reference qualification, and
constness of the forwarded expression without verbose trait boilerplate.

## Examples

Bad:

```cpp
template <typename Range>
std::ranges::range_value_t<Range>& RandomChoice(Range&& r) {
  CHECK(!r.empty());
  return r[base::RandGenerator(r.size())];
}
```

Good:

```cpp
template <typename Range>
decltype(auto) RandomChoice(Range&& r) {
  CHECK(!r.empty());
  return r[base::RandGenerator(r.size())];
}
```
