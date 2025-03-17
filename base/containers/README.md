# base/containers library

[TOC]

## What goes here

This directory contains some stdlib-like containers.

Things should be moved here that are generally applicable across the code base.
Don't add things here just because you need them in one place and think others
may someday want something similar. You can put specialized containers in your
component's directory and we can promote them here later if we feel there is
broad applicability.

### Design and naming

Fundamental [//base principles](../README.md#design-and-naming) apply, i.e.:

Containers should adhere as closely to stdlib as possible. Functions and
behaviors not present in stdlib should only be added when they are related to
the specific data structure implemented by the container.

For stdlib-like containers our policy is that they should use stdlib-like naming
even when it may conflict with the style guide. So functions and class names
should be lower case with underscores. Non-stdlib-like classes and functions
should use Google naming. Be sure to use the base namespace.

## Map and set selection

### Usage advice

1.  If you just need a generic map or set container without any additional
    properties then prefer to use `absl::flat_hash_map` and
    `absl::flat_hash_set`. These are versatile containers that have good
    performance on both large and small sized data.

    1.  Is pointer-stability of values (but not keys) required? Then use
        `absl::flat_hash_map<Key, std::unique_ptr<Value>>`.
    2.  Is pointer-stability of keys required? Then use `absl::node_hash_map`
        and `absl::node_hash_set`.

2.  If you require sorted order, then the best choice depends on whether your
    map is going to be written once and read many times, or if it is going to be
    written frequently throughout its lifetime.

    1.  If the map is written once, then `base::flat_map` and `base::flat_set`
        are good choices. While they have poor asymptotic behavior on writes, on
        a write-once container this performance is no worse than the standard
        library tree containers and so they are strictly better in terms of
        overhead.
    2.  If the map is always very small, then `base::flat_map` and
        `base::flat_set` are again good choices, even if the map is being
        written to multiple times. While mutations are O(n) this cost is
        negligible for very small values of n compared to the cost of doing a
        malloc on every mutation.
    3.  If the map is written multiple times and is large then then `std::map`
        and `std::set` are the best choices.
    4.  If you require pointer stability (on either the key or value) then
        `std::map` and `std::set` are the also the best choices.

When using `base::flat_map` and `base::flat_set` there are also fixed versions
of these that are backed by a `std::array` instead of a `std::vector` and which
don't provide mutating operators, but which are constexpr friendly and support
stack allocation. If you are using the flat structures because your container is
only written once then the fixed versions may be an even better alternative,
particularly if you're looking for a structure that can be used as a
compile-time lookup table.

Note that this advice never suggests the use of `std::unordered_map` and
`std::unordered_set`. These containers provides similar features to the Abseil
flat hash containers but with worse performance. They should only be used if
absolutely required for compatibility with third-party code.

Also note that this advice does not suggest the use of the Abseil btree
structures, `absl::btree_map` and `absl::btree_set`. This is because while these
types do provide good performance for cases where you need a sorted container
they have been found to introduce a very large code size penalty when using them
in Chromium. Until this problem can be resolved they should not be used in
Chromium code.

### Map and set implementation details

Sizes are on 64-bit platforms. Ordered iterators means that iteration occurs in
the sorted key order. Stable iterators means that iterators are not invalidated
by unrelated modifications to the container. Stable pointers means that pointers
to keys and values are not invalidated by unrelated modifications to the
container.

The table lists the values for maps, but the same properties apply to the
corresponding set types.


| Container             | Empty size | Per-item overhead | Ordered iterators? | Stable iterators? | Stable pointers? | Lookup complexity | Mutate complexity |
|:--------------------- |:---------- |:----------------- |:------------------ |:----------------- |:---------------- |:----------------- |:----------------- |
| `std::map`            | 16 bytes   | 32 bytes          | Yes                | Yes               | Yes              | O(log n)          | O(log n)          |
| `std::unordered_map`  | 128 bytes  | 16-24 bytes       | No                 | No                | Yes              | O(1)              | O(1)              |
| `base::flat_map`      | 24 bytes   | 0 bytes           | Yes                | No                | No               | O(log n)          | O(n)              |
| `absl::flat_hash_map` | 40 bytes   | 1 byte            | No                 | No                | No               | O(1)              | O(1)              |
| `absl::node_hash_map` | 40 bytes   | 1 byte            | No                 | No                | Yes              | O(1)              | O(1)              |

Note that all of these containers except for `std::map` have some additional
memory overhead based on their load factor that isn't accounted for by their
per-item overhead. This includes `base::flat_map` which doesn't have a hash
table load factor but does have the `std::vector` equivalent, unused capacity
from its double-on-resize allocation strategy.

### std::map and std::set

A red-black tree. Each inserted item requires the memory allocation of a node
on the heap. Each node contains a left pointer, a right pointer, a parent
pointer, and a "color" for the red-black tree (32 bytes per item on 64-bit
platforms).

### std::unordered\_map and std::unordered\_set

A hash table. Implemented on Windows as a `std::vector` + `std::list` and in libc++
as the equivalent of a `std::vector` + a `std::forward_list`. Both implementations
allocate an 8-entry hash table (containing iterators into the list) on
initialization, and grow to 64 entries once 8 items are inserted. Above 64
items, the size doubles every time the load factor exceeds 1.

The empty size is `sizeof(std::unordered_map)` = 64 + the initial hash table
size which is 8 pointers. The per-item overhead in the table above counts the
list node (2 pointers on Windows, 1 pointer in libc++), plus amortizes the hash
table assuming a 0.5 load factor on average.

In a microbenchmark on Windows, inserts of 1M integers into a
`std::unordered_set` took 1.07x the time of `std::set`, and queries took 0.67x
the time of `std::set`. For a typical 4-entry set (the statistical mode of map
sizes in the browser), query performance is identical to `std::set` and
`base::flat_set`. On ARM, `std::unordered_set` performance can be worse because
integer division to compute the bucket is slow, and a few "less than" operations
can be faster than computing a hash depending on the key type. The takeaway is
that you should not default to using unordered maps because "they're faster."

### base::flat\_map and base::flat\_set

A sorted `std::vector`. Seached via binary search, inserts in the middle require
moving elements to make room. Good cache locality. For large objects and large
set sizes, `std::vector`'s doubling-when-full strategy can waste memory.

Supports efficient construction from a vector of items which avoids the O(n^2)
insertion time of each element separately.

The per-item overhead will depend on the underlying `std::vector`'s reallocation
strategy and the memory access pattern. Assuming items are being linearly added,
one would expect it to be 3/4 full, so per-item overhead will be 0.25 *
sizeof(T).

`flat_set` and `flat_map` support a notion of transparent comparisons.
Therefore you can, for example, lookup `std::string_view` in a set of
`std::strings` without constructing a temporary `std::string`. This
functionality is based on C++14 extensions to the `std::set`/`std::map`
interface.

You can find more information about transparent comparisons in [the `less<void>`
documentation](https://en.cppreference.com/w/cpp/utility/functional/less_void).

Example, smart pointer set:

```cpp
// Declare a type alias using base::UniquePtrComparator.
template <typename T>
using UniquePtrSet = base::flat_set<std::unique_ptr<T>,
                                    base::UniquePtrComparator>;

// ...
// Collect data.
std::vector<std::unique_ptr<int>> ptr_vec;
ptr_vec.reserve(5);
std::generate_n(std::back_inserter(ptr_vec), 5, []{
  return std::make_unique<int>(0);
});

// Construct a set.
UniquePtrSet<int> ptr_set(std::move(ptr_vec));

// Use raw pointers to lookup keys.
int* ptr = ptr_set.begin()->get();
EXPECT_TRUE(ptr_set.find(ptr) == ptr_set.begin());
```

Example `flat_map<std::string, int>`:

```cpp
base::flat_map<std::string, int> str_to_int({{"a", 1}, {"c", 2},{"b", 2}});

// Does not construct temporary strings.
str_to_int.find("c")->second = 3;
str_to_int.erase("c");
EXPECT_EQ(str_to_int.end(), str_to_int.find("c")->second);

// NOTE: This does construct a temporary string. This happens since if the
// item is not in the container, then it needs to be constructed, which is
// something that transparent comparators don't have to guarantee.
str_to_int["c"] = 3;
```

### base::fixed\_flat\_map and base::fixed\_flat\_set

These are specializations of `base::flat_map` and `base::flat_set` that operate
on a sorted `std::array` instead of a sorted `std::vector`. These containers
have immutable keys, and don't support adding or removing elements once they are
constructed. However, these containers are constructed on the stack and don't
have any space overhead compared to a plain array. Furthermore, these containers
are constexpr friendly (assuming the key and mapped types are), and thus can be
used as compile time lookup tables.

To aid their constructions type deduction helpers in the form of
`base::MakeFixedFlatMap` and `base::MakeFixedFlatSet` are provided. While these
helpers can deal with unordered data, they require that keys are not repeated.
This precondition is CHECKed, failing compilation if this precondition is
violated in a constexpr context.

Example:

```cpp
constexpr auto kSet = base::MakeFixedFlatSet<int>({1, 2, 3});

constexpr auto kMap = base::MakeFixedFlatMap<std::string_view, int>(
    {{"foo", 1}, {"bar", 2}, {"baz", 3}});
```

Both `MakeFixedFlatSet` and `MakeFixedFlatMap` require callers to explicitly
specify the key (and mapped) type.

### absl::flat\_hash\_map and absl::flat\_hash\_set

A hash table. These use Abseil's "swiss table" design which is elaborated on in
more detail at https://abseil.io/about/design/swisstables and
https://abseil.io/docs/cpp/guides/container#hash-tables. The short version is
that it uses an open addressing scheme with a lookup scheme that is designed to
minimize memory accesses and branch mispredicts.

The flat hash map structures also store the key and value directly in the hash
table slots, eliminating the need for additional memory allocations for
inserting or removing individual nodes. The comes at the cost of eliminating
pointer stability: unlike the standard library hash tables a rehash will not
only invalidate all iterators but also all pointers to the stored elements.

In practical use these Abseil containers perform well enough that they are a
good default choice for a map or set container when you don't have any stronger
constraints. In fact, even when you require value pointer-stability it is still
generally better to wrap the value in a `std::unique_ptr` than to use an
alternative structure that provides such stability directly.

### absl::node\_hash\_map and absl::node\_hash\_set

A variant of the Abseil hash maps that stores the key-value pair in a separately
allocated node rather than directly in the hash table slots. This guarantees
pointer-stability for both the keys and values in the table, invalidating them
only when the element is deleted, but it comes at the cost of requiring an
additional allocation for every element inserted.

There are two main uses for this structure. One is for cases where you require a
map with pointer-stability for the key (not the value), which cannot be done
with the Abseil flat map or set. The other is for cases where you want a drop-in
replacement for an existing `std::unordered_map` or `std::unordered_set` and you
aren't sure if pointer-stability is required. If you know that pointer-stability
is unnecessary then it would be better to convert to the flat tables but this
may be difficult to prove when working on unfamiliar code or doing a large scale
change. In such cases the node hash maps are still generally superior to the
standard library maps.

## Deque

### Usage advice

Chromium code should always use `base::circular_deque` or `base::queue` in
preference to `std::deque` or `std::queue` due to memory usage and platform
variation.

The `base::circular_deque` implementation (and the `base::queue` which uses it)
provide performance consistent across platforms that better matches most
programmer's expectations on performance (it doesn't waste as much space as
libc++ and doesn't do as many heap allocations as MSVC). It also generates less
code than `std::queue`: using it across the code base saves several hundred
kilobytes.

Since `base::deque` does not have stable iterators and it will move the objects
it contains, it may not be appropriate for all uses. If you need these,
consider using a `std::list` which will provide constant time insert and erase.

### std::deque and std::queue

The implementation of `std::deque` varies considerably which makes it hard to
reason about. All implementations use a sequence of data blocks referenced by
an array of pointers. The standard guarantees random access, amortized
constant operations at the ends, and linear mutations in the middle.

In Microsoft's implementation, each block is the smaller of 16 bytes or the
size of the contained element. This means in practice that every expansion of
the deque of non-trivial classes requires a heap allocation. libc++ (on Android
and Mac) uses 4K blocks which eliminates the problem of many heap allocations,
but generally wastes a large amount of space (an Android analysis revealed more
than 2.5MB wasted space from deque alone, resulting in some optimizations).
libstdc++ uses an intermediate-size 512-byte buffer.

Microsoft's implementation never shrinks the deque capacity, so the capacity
will always be the maximum number of elements ever contained. libstdc++
deallocates blocks as they are freed. libc++ keeps up to two empty blocks.

### base::circular_deque and base::queue

A deque implemented as a circular buffer in an array. The underlying array will
grow like a `std::vector` while the beginning and end of the deque will move
around. The items will wrap around the underlying buffer so the storage will
not be contiguous, but fast random access iterators are still possible.

When the underlying buffer is filled, it will be reallocated and the constents
moved (like a `std::vector`). The underlying buffer will be shrunk if there is
too much wasted space (_unlike_ a `std::vector`). As a result, iterators are
not stable across mutations.

## Stack

`std::stack` is like `std::queue` in that it is a wrapper around an underlying
container. The default container is `std::deque` so everything from the deque
section applies.

Chromium provides `base/containers/stack.h` which defines `base::stack` that
should be used in preference to `std::stack`. This changes the underlying
container to `base::circular_deque`. The result will be very similar to
manually specifying a `std::vector` for the underlying implementation except
that the storage will shrink when it gets too empty (vector will never
reallocate to a smaller size).

Watch out: with some stack usage patterns it's easy to depend on unstable
behavior:

```cpp
base::stack<Foo> stack;
for (...) {
  Foo& current = stack.top();
  DoStuff();  // May call stack.push(), say if writing a parser.
  current.done = true;  // Current may reference deleted item!
}
```

## Safety

Code throughout Chromium, running at any level of privilege, may directly or
indirectly depend on these containers. Much calling code implicitly or
explicitly assumes that these containers are safe, and won't corrupt memory.
Unfortunately, [such assumptions have not always proven
true](https://bugs.chromium.org/p/chromium/issues/detail?id=817982).

Therefore, we are making an effort to ensure basic safety in these classes so
that callers' assumptions are true. In particular, we are adding bounds checks,
arithmetic overflow checks, and checks for internal invariants to the base
containers where necessary. Here, safety means that the implementation will
`CHECK`.

As of 8 August 2018, we have added checks to the following classes:

- `base::span`
- `base::RingBuffer`
- `base::small_map`

Ultimately, all base containers will have these checks.

### Safety, completeness, and efficiency

Safety checks can affect performance at the micro-scale, although they do not
always. On a larger scale, if we can have confidence that these fundamental
classes and templates are minimally safe, we can sometimes avoid the security
requirement to sandbox code that (for example) processes untrustworthy inputs.
Sandboxing is a relatively heavyweight response to memory safety problems, and
in our experience not all callers can afford to pay it.

(However, where affordable, privilege separation and reduction remain Chrome
Security Team's first approach to a variety of safety and security problems.)

One can also imagine that the safety checks should be passed on to callers who
require safety. There are several problems with that approach:

- Not all authors of all call sites will always
  - know when they need safety
  - remember to write the checks
  - write the checks correctly
  - write the checks maximally efficiently, considering
    - space
    - time
    - object code size
- These classes typically do not document themselves as being unsafe
- Some call sites have their requirements change over time
  - Code that gets moved from a low-privilege process into a high-privilege
    process
  - Code that changes from accepting inputs from only trustworthy sources to
    accepting inputs from all sources
- Putting the checks in every call site results in strictly larger object code
  than centralizing them in the callee

Therefore, the minimal checks that we are adding to these base classes are the
most efficient and effective way to achieve the beginning of the safety that we
need. (Note that we cannot account for undefined behavior in callers.)
