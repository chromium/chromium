---
name: general/prefer-gmock-elements-are-for-container-assertions
rubric: readability
severity: nit
---

**Rule.** In unit tests, use GMock container matchers such as
EXPECT_THAT(container, testing::ElementsAre(...)) or
testing::ElementsAreArray(...) (or UnorderedElementsAre) when verifying
container elements, rather than asserting container .size() and indexing
individual elements via operator[] or .at().

**Why.** Using container matchers eliminates repetitive size and index
boilerplate, guarantees atomic validation of container size and ordered
elements, and outputs descriptive diagnostic diffs of mismatched elements and
sizes upon failure.

## Examples

Bad:

```cpp
ASSERT_EQ(2u, values_freed.size());
EXPECT_EQ(kFirst, values_freed[0]);
EXPECT_EQ(kSecond, values_freed[1]);
```

Good:

```cpp
EXPECT_THAT(values_freed, testing::ElementsAre(kFirst, kSecond));
```
