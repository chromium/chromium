---
name: general/place-file-local-types-and-constants-at-top-of-tu
rubric: readability
severity: nit
---

**Rule.** Place translation-unit-scoped constants, type aliases, and shared
structs/enums at the top of the anonymous namespace in `.cc` files before
function implementations, rather than scattering them mid-file between function
bodies. Additionally, place single-function helper types (such as result enums
or parameter structs used exclusively by one helper) immediately adjacent to the
function that consumes them, rather than separating unrelated file-scope
constants.

**Why.** Grouping shared constants and types at the top of the anonymous
namespace establishes a predictable file layout and prevents symbols from being
buried between function implementations. Colocating single-function helper types
with their consuming functions keeps file-scope constants unified and improves
locality of reference for dedicated helpers.

## Examples

Bad:

```cpp
namespace {

// Buried mid-file between helper function implementations:
void HelperOne() { ... }
struct InternalData { int id; };
void HelperTwo() { ... }

// Separating unrelated file-scope constants with a single-function enum:
enum class RecordResult { kSuccess = 0, kFailure = 1 };
constexpr std::wstring_view kDefaultPrefix = L"Temp";
void RecordResultMetric(RecordResult r) { ... }

}  // namespace
```

Good:

```cpp
namespace {

// Shared constants and file-wide types grouped at top:
constexpr std::wstring_view kDefaultPrefix = L"Temp";
struct InternalData { int id; };

void HelperOne() { ... }
void HelperTwo() { ... }

// Single-function helper type colocated directly with its consuming function:
enum class RecordResult { kSuccess = 0, kFailure = 1 };
void RecordResultMetric(RecordResult r) { ... }

}  // namespace
```
