---
name: general/import-classes-instead-of-fully-qualified-names
rubric: readability
severity: nit
---

**Rule.** Import classes instead of writing fully qualified class names inline
(`org.chromium.foo.Bar`), including in annotation arguments. Use an inline fully
qualified name only when it is needed, for example to disambiguate two classes
with the same simple name. Import inner classes with descriptive names directly;
keep `OuterClass.InnerClass` only for generically named inner classes such as
`Observer`.

**Why.** `styleguide/java/java.md` ("Class Qualification and Imports") prefers
imports, because inline qualified names make lines long and hide a file's
dependencies from its import list.

## Examples

Bad:

```java
private final org.chromium.base.supplier.ObservableSupplierImpl<Boolean> mSupplier =
        new org.chromium.base.supplier.ObservableSupplierImpl<>();
```

Good:

```java
import org.chromium.base.supplier.ObservableSupplierImpl;

private final ObservableSupplierImpl<Boolean> mSupplier = new ObservableSupplierImpl<>();
```
