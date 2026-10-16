---
name: general/test-multi-word-boundaries-for-bitsets-and-enum-sets
rubric: testing
severity: should_fix
---

**Rule.** When writing unit tests for bitsets, enum sets (such as
`base::EnumSet` or `std::bitset`), or bit-vector containers, include test cases
for operations that cross machine-word boundaries (such as ranges spanning
across 64-bit boundaries). Verify that range insertion, removal, masking, and
queries function correctly across multi-word boundaries in both runtime and
constexpr contexts.

**Why.** Containers backed by bitmasks or multi-word integer arrays use
different indexing, bit-shifting, and masking logic when operations cross 64-bit
machine-word boundaries. Testing only single-word sets (\<= 64 elements) leaves
multi-word bitwise logic unexercised and can mask off-by-one errors, truncation,
or incorrect word-carry logic.

## Examples

Bad:

```cpp
// Only tests single-word range operations (<= 64 values).
TEST_F(EnumSetTest, PutRange) {
  TestEnumSet enums;
  enums.PutRange(TestEnum::kTest2, TestEnum::kTest4);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest2, TestEnum::kTest3, TestEnum::kTest4}),
            enums);
}
```

Good:

```cpp
// Tests PutRange spanning across 64-bit word boundaries (> 64 values).
TEST_CONSTEXPRABLE_F(EnumSetTest, SparseEnumPutRange) {
  enum class TestEnumSparse {
    kTest1 = 1,
    kTestMin = 1,
    kTest50 = 50,
    kTest100 = 100,
    kTestMax = kTest100,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;
  TestEnumSparseSet sparse;

  EXPECT_CX_EQ(sparse.GetNth64bitWordBitmask(0), 0ull);
  sparse.PutRange(TestEnumSparse::kTest50, TestEnumSparse::kTest100);
  EXPECT_CX_EQ(sparse.GetNth64bitWordBitmask(0), 0xFFFC000000000000ull);
  EXPECT_CX_EQ(sparse.GetNth64bitWordBitmask(1), 0x1FFFFFFFFFull);
}
TEST_CONSTEXPRABLE_F_CHECK(EnumSetTest, SparseEnumPutRange)
```
