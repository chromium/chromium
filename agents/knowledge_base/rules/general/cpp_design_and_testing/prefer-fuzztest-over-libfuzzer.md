---
name: general/prefer-fuzztest-over-libfuzzer
rubric: design
severity: nit
---

**Rule.** Prefer FuzzTest (`FUZZ_TEST`) over legacy `libFuzzer` (`fuzzer_test`)
and `libprotobuf-mutator` when writing new fuzz targets. Use `libFuzzer` only
when maintaining existing targets, and leverage FuzzTest domains
(`fuzztest::Arbitrary<T>()`, `fuzztest::InRegexp`, etc.) for structured or
grammar-aware inputs instead of protobuf mutators.

**Why.** libFuzzer is deprecated in Chromium for new fuzz targets. FuzzTest
integrates seamlessly with GTest, simplifies fuzz target authoring, and provides
expressive domains with superior mutation and grammar-aware fuzzing strategies
without requiring separate protobuf mutator boilerplate.

## Examples

Bad:

```cpp
// Adding a new grammar-aware fuzzer using legacy libprotobuf-mutator:
fuzzer_test("my_proto_fuzzer") {
  sources = [ "my_proto_fuzzer.cc" ]
  deps = [ ":my_proto", "//third_party/libprotobuf-mutator" ]
}

DEFINE_BINARY_PROTO_FUZZER(const MyProto& proto) {
  TestTarget(proto);
}
```

Good:

```cpp
// Using FuzzTest with domains instead:
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

void MyFuzzTest(const MyProto& proto) {
  TestTarget(proto);
}
FUZZ_TEST(MySuite, MyFuzzTest)
    .WithDomains(fuzztest::Arbitrary<MyProto>());
```
