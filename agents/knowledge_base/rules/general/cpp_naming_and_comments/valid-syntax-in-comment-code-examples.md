---
name: general/valid-syntax-in-comment-code-examples
rubric: readability
severity: should_fix
---

**Rule.** Ensure C++ code snippets, example implementations, and usage blocks in
documentation comments are syntactically valid and use accurate types. Terminate
all statements and return expressions with semicolons, close all matching
delimiters, and use concrete types (such as size_t Extent) rather than typename
for non-type template parameters.

**Why.** Code examples in header comments and documentation are not verified by
compilers or formatted by automated linters, but developers frequently copy or
adapt them. Incomplete syntax or incorrect template parameter types cause
compilation errors and confusion when copied or referenced.

## Examples

Bad:

```cpp
// Example usage:
//   IntCallback ProgressObserver::GetUpdateProgressCallback() {
//     return base::BindRepeating(&ProgressObserver::UpdateProgress,
//                                weak_ptr_factory_.GetWeakPtr())
//   }

//   template <typename Extent>
//   void ProcessMemory(base::span<const uint8_t, Extent>);
```

Good:

```cpp
// Example usage:
//   IntCallback ProgressObserver::GetUpdateProgressCallback() {
//     return base::BindRepeating(&ProgressObserver::UpdateProgress,
//                                weak_ptr_factory_.GetWeakPtr());
//   }

//   template <size_t Extent>
//   void ProcessMemory(base::span<const uint8_t, Extent>);
```
