---
name: general/compare-optional-to-nullopt-in-gtest-assertions
rubric: readability
severity: optional
---

**Rule.** In GoogleTest assertions, test whether an std::optional is engaged or
disengaged by comparing it directly against std::nullopt (e.g., using
EXPECT_NE(opt, std::nullopt) or EXPECT_EQ(opt, std::nullopt)) or GMock matchers
rather than checking .has_value() with EXPECT_TRUE/EXPECT_FALSE.

**Why.** Comparing an std::optional directly against std::nullopt allows
GoogleTest to print the optional's actual state and contained value in test
failure diagnostics. Checking .has_value() collapses failure reports to an
uninformative boolean true/false mismatch.

## Examples

Bad:

```cpp
EXPECT_TRUE(info->bytes_per_sector.has_value());
EXPECT_FALSE(result.has_value());
```

Good:

```cpp
EXPECT_NE(info->bytes_per_sector, std::nullopt);
EXPECT_EQ(result, std::nullopt);
```
