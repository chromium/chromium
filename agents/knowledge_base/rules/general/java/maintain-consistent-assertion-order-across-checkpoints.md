---
name: general/maintain-consistent-assertion-order-across-checkpoints
rubric: readability
severity: nit
---

**Rule.** When checking the state of multiple variables, instances, or
properties across successive checkpoints or before/after state transitions
within a test case, maintain the same relative assertion order in each check
block. Do not permute or invert the sequence of assertions between checkpoints.

**Why.** Consistent ordering across verification blocks reduces cognitive
friction, makes it easy to visually diff and compare state transitions, and
prevents confusing readers about which variable or object is being verified.

## Examples

Bad:

```cpp
AutoReset<int> resetter1{&value1, 20};
AutoReset<int> resetter2{&value2, 200};
EXPECT_EQ(20, value1);
EXPECT_EQ(200, value2);

resetter2 = std::move(resetter1);
EXPECT_EQ(100, value2);
EXPECT_EQ(20, value1);
```

Good:

```cpp
AutoReset<int> resetter1{&value1, 20};
AutoReset<int> resetter2{&value2, 200};
EXPECT_EQ(20, value1);
EXPECT_EQ(200, value2);

resetter2 = std::move(resetter1);
EXPECT_EQ(20, value1);
EXPECT_EQ(100, value2);
```
