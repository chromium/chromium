---
name: general/provide-matching-deduction-guides-for-class-template-constru
rubric: correctness
severity: should_fix
---

**Rule.** When adding constructor overloads (such as tag-disambiguated
constructors or iterator/range parameters) to a class template that uses
explicit deduction guides, provide corresponding Class Template Argument
Deduction (CTAD) deduction guides for the new constructor signatures.

**Why.** Class templates with explicit deduction guides do not synthesize
implicit deduction guides for new constructor overloads with dependent or
concept-constrained types. Omitting matching deduction guides causes CTAD to
fail at call sites or deduce incorrect template arguments, forcing callers to
spell out redundant template parameters.

## Examples

Bad:

```cpp
// Constructor added to class template:
template <typename It, typename Size>
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag, It first, Size count);

// Missing deduction guide: CTAD fails when callers write span(kUnchecked, ptr, count).
```

Good:

```cpp
// Constructor added to class template:
template <typename It, typename Size>
UNSAFE_BUFFER_USAGE constexpr span(unchecked_t tag, It first, Size count);

// Corresponding deduction guide added:
template <typename It, typename EndOrSize>
  requires(std::contiguous_iterator<It>)
span(unchecked_t, It, EndOrSize)
    -> span<std::remove_reference_t<std::iter_reference_t<It>>,
            internal::MaybeStaticExt<EndOrSize>>;
```
