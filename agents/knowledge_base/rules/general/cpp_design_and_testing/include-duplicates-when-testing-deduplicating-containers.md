---
name: general/include-duplicates-when-testing-deduplicating-containers
rubric: testing
severity: should_fix
---

**Rule.** When writing unit tests for unique associative containers (such as
base::flat_set, base::flat_map, std::set, or absl::flat_hash_set),
range/iterator constructors (such as std::from_range or std::ranges::to), or
deduplication algorithms (such as sort_and_unique), include duplicate elements
in the test inputs—especially when testing move-only types (such as
MoveOnlyInt). Verify that the container correctly deduplicates entries,
maintains the expected size and element ordering, properly destroys duplicate
elements, and handles move semantics without leaving corrupted state or dangling
resources.

**Why.** Providing only distinct elements in unit tests for unique associative
containers or deduplication routines leaves duplicate-elimination logic
unexercised. For move-only types, deduplication requires proper element
destruction, lifetime management, and move operations; testing with duplicate
inputs prevents masked memory leaks, incorrect element counts, or improper
cleanup of discarded duplicates.

## Examples

Bad:

```cpp
// Testing container initialization or range constructors with only distinct elements:
std::vector<MoveOnlyInt> input_vec;
input_vec.emplace_back(1);
input_vec.emplace_back(2);
input_vec.emplace_back(3);

// Fails to exercise duplicate elimination or move-only destruction of duplicates:
flat_set<MoveOnlyInt> original(std::move(input_vec));
EXPECT_EQ(3u, original.size());

std::list<MoveOnlyInt> input_list;
input_list.emplace_back(1);
input_list.emplace_back(2);
flat_set<MoveOnlyInt> from_range_set(std::from_range,
                                    base::RangeAsRvalues(std::move(input_list)));
```

Good:

```cpp
// Includes duplicate elements to verify deduplication and move-only cleanup:
std::vector<MoveOnlyInt> input_vec;
input_vec.emplace_back(1);
input_vec.emplace_back(1);  // Duplicate entry.
input_vec.emplace_back(2);
input_vec.emplace_back(3);
input_vec.emplace_back(3);  // Duplicate entry.

flat_set<MoveOnlyInt> original(std::move(input_vec));
EXPECT_EQ(3u, original.size());

std::list<MoveOnlyInt> input_list;
input_list.emplace_back(1);
input_list.emplace_back(1);  // Duplicate entry.
input_list.emplace_back(2);
flat_set<MoveOnlyInt> from_range_set(std::from_range,
                                    base::RangeAsRvalues(std::move(input_list)));
EXPECT_EQ(2u, from_range_set.size());
```
