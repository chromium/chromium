# `base::ranges`

This directory aims to implement a C++14 version of the new `std::ranges`
algorithms that were introduced in C++20. These implementations are added to the
`::base::ranges` namespace, and callers can access them by including
[`base/ranges/algorithm.h`](https://source.chromium.org/chromium/chromium/src/+/main:base/ranges/algorithm.h).

## Similarities with C++20:

### Automatically deducing `begin()` and `end()`
As probably one of the most important changes for readability and usability, all
algorithms in `base::ranges` have overloads for ranges of elements, which allow
callers to no longer specify `begin()` and `end()` iterators themselves.

Before:
```c++
bool HasEvens(const std::vector<int>& vec) {
  return std::any_of(vec.begin(), vec.end(), [](int i) { return i % 2 == 0; });
}
```

After:
```c++
bool HasEvens(const std::vector<int>& vec) {
  return base::ranges::any_of(vec, [](int i) { return i % 2 == 0; });
}
```

Furthermore, these overloads also support binding to temporaries, so that
applying algorithms to return values is easier:

```c++
std::vector<int> GetNums();
```

Before:

```c++
bool HasEvens() {
  std::vector<int> nums = GetNums();
  return std::any_of(nums.begin(), nums.end(),
                     [](int i) { return i % 2 == 0; });
}
```

After:
```c++
bool HasEvens() {
  return base::ranges::any_of(GetNums(), [](int i) { return i % 2 == 0; });
}
```

### Support for Projections
In addition to supporting automatically deducing the `begin()` and `end()`
iterator for ranges, the `base::ranges::` algorithms also support projections,
that can be applied to arguments prior to passing it to supplied transformations
or predicates. This is especially useful when ordering a collection of classes
by a specific data member of the class. Example:

Before:
```cpp
std::sort(suggestions->begin(), suggestions->end(),
          [](const autofill::Suggestion& a, const autofill::Suggestion& b) {
            return a.match < b.match;
          });
```

After:
```cpp
base::ranges::sort(*suggestions, /*comp=*/{}, &autofill::Suggestion::match);
```

Anything that is callable can be used as a projection. This includes
`FunctionObjects` like function pointers or functors, but also pointers to
member function and pointers to data members, as shown above. When not specified
a projection defaults to `base::ranges::identity`, which simply perfectly
forwards its argument.

Projections are supported in both range and iterator-pair overloads of the
`base::ranges::` algorithms, for example `base::ranges::all_of` has the
following signatures:

```cpp
template <typename InputIterator, typename Pred, typename Proj = identity>
bool all_of(InputIterator first, InputIterator last, Pred pred, Proj proj = {});

template <typename Range, typename Pred, typename Proj = identity>
bool all_of(Range&& range, Pred pred, Proj proj = {});
```

## Differences from C++20:
To simplify the implementation of the `base::ranges::` algorithms, they dispatch
to the `std::` algorithms found in C++14. This leads to the following list of
differences from C++20. Since most of these differences are differences in the
library and not in the language, they could be addressed in the future by adding
corresponding implementations.

### Lack of Constraints
Due to the lack of support for concepts in the language, the algorithms in
`base::ranges` do not have the constraints that are present on the algorithms in
`std::ranges`. Instead, they support any type, much like C++14's `std::`
algorithms. In the future this might be addressed by adding corresponding
constraints via SFINAE, should the need arise.

### Lack of Range Primitives
Due to C++14's lack of `std::ranges` concepts like sentinels and other range
primitives, algorithms taking a `[first, last)` pair rather than a complete
range, do not support different types for `first` and `last`. Since they rely on
C++14's implementation, the type must be the same. This could be addressed in
the future by implementing support for sentinel types ourselves.

### Lack of `constexpr`
The `base::ranges` algorithms can only be used in a `constexpr` context when
they call underlying `std::` algorithms that are themselves `constexpr`.  Before
C++20, only `std::min`, `std::max` and `std::minmax` are annotated
appropriately, so code like `constexpr bool foo = base::ranges::any_of(...);`
will fail because the compiler will not find a `constexpr std::any_of`.  This
could be addressed by either upgrading Chromium's STL to C++20, or implementing
`constexpr` versions of some of these algorithms ourselves.

### Lack of post C++14 algorithms
Since most algorithms in `base::ranges` dispatch to their C++14 equivalent, some
`std::` algorithms that are not present in C++14 have no implementation in
`base::ranges`. This list of algorithms includes the following:

- [`std::sample`](https://en.cppreference.com/w/cpp/algorithm/sample) (added in C++17)

### Return Types
Some of the algorithms in `std::ranges::` have different return types than their
equivalent in `std::`. For example, while `std::for_each` returns the passed-in
`Function`, `std::ranges::for_each` returns a `std::ranges::for_each_result`,
consisting of the `last` iterator and the function.

In the cases where the return type differs, `base::ranges::` algorithms will
continue to return the old return type.

### No blocking of ADL
The algorithms defined in `std::ranges` are not found by ADL, and inhibit ADL
when found by [unqualified name lookup][1]. This is done to be able to enforce
the constraints specified by those algorithms and commonly implemented by using
function objects instead of regular functions. Since we don't support
constrained algorithms yet, we don't implement the blocking of ADL either.

[1]: https://wg21.link/algorithms.requirements#2
