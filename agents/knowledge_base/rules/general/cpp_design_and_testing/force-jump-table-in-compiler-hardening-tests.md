---
name: general/force-jump-table-in-compiler-hardening-tests
rubric: testing
severity: should_fix
---

**Rule.** When writing compiler hardening or sanitizer tests (such as for
-fsanitize=return) intended to exercise switch jump tables rather than static
lookup tables, ensure the switch generates a jump table by providing at least 4
dense case labels and returning non-constant values (e.g., calling distinct
NOINLINE functions) instead of literal constants.

**Why.** Compilers optimize switch statements that return constant values—even
when wrapped in helper types like std::optional<T>—into static lookup tables. To
specifically exercise compiler sanitizers and hardening checks along jump-table
code paths, tests must meet compiler jump-table heuristics (such as sufficient
case cluster density) and prevent constant folding with non-constant return
expressions.

## Examples

Bad:

```cpp
// Returns constants directly, causing the compiler to emit a static lookup table:
std::optional<int> MaybeGetIntFromMyEnum(MyEnum val) {
  switch (val) {
    case MyEnum::kOne:
      return 1;
    case MyEnum::kTwo:
      return 2;
    case MyEnum::kThree:
      return 3;
    case MyEnum::kFour:
      return 4;
  }
}
```

Good:

```cpp
NOINLINE int FuncOne() { return 1; }
NOINLINE int FuncTwo() { return 2; }
NOINLINE int FuncThree() { return 3; }
NOINLINE int FuncFour() { return 4; }

// Generates a jump table by providing >=4 dense cases returning non-constants:
NOINLINE int GetIntFromMyEnumJumpTable(MyEnum val) {
  switch (val) {
    case MyEnum::kOne:
      return FuncOne();
    case MyEnum::kTwo:
      return FuncTwo();
    case MyEnum::kThree:
      return FuncThree();
    case MyEnum::kFour:
      return FuncFour();
  }
}
```
