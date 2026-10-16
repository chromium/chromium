---
name: general/document-positional-ordering-contracts-for-untyped-collectio
rubric: readability
severity: nit
---

**Rule.** When extracting values by positional index from untyped collections
(such as std::vector, base::span, or std::array) or tuples whose ordering is
defined by a paired producer, serializer, or companion method, add an
explanatory comment documenting the ordering contract. State the assumed element
mapping and cite the paired producer or serialization format to keep producer
and consumer synchronized during future refactorings.

**Why.** Raw positional indices (such as `values[0]`) conceal the semantic
meaning of elements and depend on implicit contracts with the producer.
Documenting the ordering contract prevents desynchronization when either the
producer or consumer is modified and clarifies non-obvious element mappings for
readers.

## Examples

Bad:

```cpp
void SetRegisters(RegisterContext* thread_context,
                  const std::vector<uintptr_t>& registers) override {
  SetRegisterContextStackPointer(thread_context, registers[0]);
}
```

Good:

```cpp
void SetRegisters(RegisterContext* thread_context,
                  const std::vector<uintptr_t>& registers) override {
  // Extraction of register values from `registers` must be in sync with
  // the order used in `GetRegisters` above.
  SetRegisterContextStackPointer(thread_context, registers[0]);
}
```
