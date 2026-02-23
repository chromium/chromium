---
name: fuzzing
description: Implements, registers, and verifies fuzz tests in Chromium. Use when the user asks to add or write fuzzers in C++, or mentions fuzz testing or FUZZ_TEST.
---

# Fuzzing (Chromium)

## 1. Setup

Ensure the output directory is configured:

```bash
gn gen out/fuzz --args='enable_fuzztest_fuzz=true is_debug=false is_asan=true \
is_component_build=false use_remoteexec=true'
```

### 2. Implement the FUZZ_TEST

Add to `*_unittest.cc` alongside existing tests:

```cpp
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

// 1. Define the property function
void MyPropertyFunction(int i, const std::string& s) {
  // Call code under test. Focus on functions parsing untrusted input, complex
  // state machines, or data processing.
  bool result = MyComponent::DoSomething(i, s);

  // Add test assertions about invariants (e.g. "roundtrip equality", "valid
  // output structure"). Sanitizers like ASAN catch crashes.
  EXPECT_TRUE(result);
}

// 2. Register with FUZZ_TEST macro
FUZZ_TEST(MyComponentFuzzTest, MyPropertyFunction)
  .WithDomains(
      fuzztest::InRange(0, 100),
      fuzztest::Arbitrary<std::string>()
  );
```

For complex types:

- **Construct from primitives:** If the object has a parsing constructor (e.g.
  `GURL(string)`), accept the primitive and construct it inside your test
  function.
- **Define a local domain:** Use `fuzztest::Constructor` or `fuzztest::Map` to
  build valid objects.
  ```cpp
  auto ArbitraryFoo() {
    return fuzztest::Constructor<Foo>(fuzztest::InRange(0, 10));
  }
  ```

### 3. Register in BUILD.gn

You **MUST** register the test in the `fuzztests` list (in alphabetical order)
of the **executable** `test` target.

**Case A: File is in a `test()` target:** Add
`//third_party/fuzztest:fuzztest_gtest_main` to `deps`.

```gn
test("my_component_unittests") {
  sources = [ "my_component_unittest.cc" ]

  # Format: SuiteName.TestName
  fuzztests = [
    "MyComponentFuzzTest.MyPropertyFunction",
  ]

  deps = [
    # ... existing deps ...
    "//third_party/fuzztest:fuzztest_gtest_main",
  ]
}
```

**Case B: File is in a `source_set()`:** Add `//third_party/fuzztest:fuzztest`
to `deps`.

```gn
source_set("tests") {
  sources = [ "my_component_unittest.cc" ]
  deps = [
    "//third_party/fuzztest:fuzztest",
    # ...
  ]
}
```

Find the executable `test()` target that depends on this `source_set()`:
`gn refs out/fuzz //path/to:source_set --testonly=true --type=executable --all`

Then, ensure it lists the fuzz test in its `fuzztests` variable (in alphabetical
order).

### 4. Mandatory verification workflow

The task is **incomplete** until you successfully execute this sequence:

1. **Build**

```bash
autoninja --quiet -C out/fuzz my_component_unittests
```

2. Verify unit tests pass

```bash
./out/fuzz/my_component_unittests \
--gtest_filter="MyComponentFuzzTest.MyPropertyFunction"
```

3. Verify fuzzing mode doesn't crash

```bash
./out/fuzz/my_component_unittests \
--fuzz="MyComponentFuzzTest.MyPropertyFunction" --fuzz_for=10s
```

## Resources

- **Chromium Guide**: `testing/libfuzzer/getting_started.md`
- **Macro Usage**: `third_party/fuzztest/src/doc/fuzz-test-macro.md`
- **Domains**: `third_party/fuzztest/src/doc/domains-reference.md`
- **Fixtures**: `third_party/fuzztest/src/doc/fixtures.md`
