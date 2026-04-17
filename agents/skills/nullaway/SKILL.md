---
name: nullaway
description: |-
  Guide for resolving NullAway static analysis errors.
  Best practices for:
  - Passing ObservableSupplier/Supplier<@Nullable T>
  - Dereferencing potentially @Nullable values
  - Adding @NullMarked to Java code
---

## Core Principles

### 1. Migration to `@NullMarked` vs New Code

The approach differs significantly depending on whether you are migrating
existing code to `@NullMarked` or writing new code in an already `@NullMarked`
context:

- **Migration to `@NullMarked`**:
  - **Goal**: Satisfy the static analyzer while **minimizing functional
    changes** to avoid regressions.
  - **Practice**: Use `NullUtil.assumeNonNull()` when dereferencing immediately
    to satisfy the analyzer without runtime overhead. Use `assert` when storing
    or returning values. Use `if` guards occasionally if it helps avoid
    assertions, but avoid changing the logic.
- **New Code (and modifying already `@NullMarked` code)**:
  - **Goal**: Build robust, null-safe components from the ground up.
  - **Practice**: Be more liberal with `assert`s to enforce contracts. Rely on
    correct annotations (or their absence for implicit `@NonNull`) and only use
    null guards for `@Nullable` values. `assumeNonNull()` should generally not
    be used for new code.

### 2. No Functional Changes (For Migration)

- **Rule**: When migrating existing code, avoid adding new throwing runtime
  checks where they did not exist before, unless guided by the rules below.

### 3. `assumeNonNull` vs `assert != null`

The choice between `assumeNonNull` and Java `assert` depends on how the value is
used:

- **Dereferenced right away (For Migrations)**: Use `NullUtil.assumeNonNull(x)`.
  - Example:
    ```java
    var value = mSupplier.get();
    assumeNonNull(value);
    value.doSomething();
    ```
  - *Why*: If it is null, it will NPE on the dereference anyway. `assumeNonNull`
    is a no-op that satisfies the analyzer without adding redundant runtime
    checks.
  - **Style Rule**: `assumeNonNull()` should be on a separate line, almost
    always, rather than used inline.
  - **Note**: This rule is primarily for **migrations** to avoid functional
    changes. For **new code**, rely on correct annotations and avoid
    `assumeNonNull()`.
- **Returned, passed, or stored**: Use Java `assert x != null;` before the
  operation.
  - Example:
    ```java
    var value = mSupplier.get();
    assert value != null;
    return value;
    ```
  - *Why*: This adds a runtime check (active in tests/debug) which is an
    acceptable functional change. We rely on instrumentation tests to validate
    these changes.
  - **Deep Investigation Rule**: Before asserting or assuming non-null for a
    passed value, investigate the call tree. If the method being called can be
    updated to consider the parameter `@Nullable`, prefer updating the method
    signature over adding an assertion.
  - **Warning**: Be careful with `assert value != null` on `Supplier.get()`
    during initialization. If the supplier value is set LATER (as is common with
    UI wiring), the assert might fail immediately during construction. In such
    cases, the getter should return `@Nullable` and callers should handle it,
    rather than asserting non-null immediately.

### 4. Handling Suppliers and Generics

- **Problem**: Passing a `Supplier<@Nullable T>` to a constructor that expects
  `Supplier<T>` (non-nullable), or vice versa.
- **Preference**: Prefer using exact types like `Supplier<@Nullable T>` rather
  than wildcards like `Supplier<? extends @Nullable T>` in method signatures and
  fields.
- **Upcasting**: `SupplierUtils.upcast()` is strictly for upcasting the type
  parameter to a base class (e.g., `Supplier<DerivedT>` to `Supplier<BaseT>`).
  Do NOT use it solely for handling nullability differences (e.g., `Supplier<T>`
  to `Supplier<@Nullable T>`).
- **Handling Nullability Generic Invariance**: You generally shouldn't need to
  use lambdas or upcast to bridge nullability differences (e.g., passing
  `Supplier<T>` to `Supplier<@Nullable T>`) if you are passing around subclasses
  of `ObservableSupplier`.
- **Design Principle**: If a supplier can return null, the receiver class *must*
  be updated to accept `Supplier<@Nullable T>` and handle the nullity. Never use
  hacks or assertions to force a `Supplier<@Nullable T>` to act as a
  non-nullable `Supplier<T>`.
- **Good Fix**: Alter the receiver class to accept `Supplier<@Nullable T>`.
  - Then, handle the nullability inside the receiver class using the rules above
    (`assumeNonNull` or `assert`).
- **Lazy Suppliers and Assertions**: When a getter is passed as a Supplier to be
  used later, and the value might be null at construction time but expected to
  be non-null when used:
  - Avoid asserting in the getter itself if that getter is called during
    construction to create the supplier.
  - Instead, make the getter return `@Nullable`.
  - In the caller, if the receiver expects a non-null supplier, add the
    assertion *inside* the supplier lambda (e.g.,
    `() -> { var x = getter(); assert x != null; return x; }`). This defers the
    assertion until the supplier is resolved.
- **Supplier Argument Types**: Consider changing Supplier arguments to
  `Supplier<@Nullable T>` or `MonotonicObservableSupplier<T>` in method
  signatures to avoid forcing non-nullability on callers.

### 5. Annotations Placement

- **`@NullMarked`**: Apply to the class level when you are ready to make the
  whole class null-safe.
- **`@Nullable`**: Apply to fields, parameters, and return types that can be
  null.
- **`@SuppressWarnings("NullAway")`**:
  - Use as a last resort.
  - **Do NOT add to constructors**. Fix the warnings in the constructor instead.
  - It is acceptable to add to `destroy` or `onDestroy` methods if circular
    references or cleanup order make it hard to satisfy the analyzer without
    functional changes.

### 6. Constructor Parameters for Nullable Fields

- **Rule**: If a constructor parameter is stored directly into a `@Nullable`
  field, the parameter itself should usually be marked `@Nullable` as well, even
  if it is not immediately used as nullable in the constructor. This avoids
  artificial non-null requirements at construction time.

### 7. Deciding on @Nullable for Getters

- **Rule**: When deciding whether to make a getter return `@Nullable`, look at
  how callers handle the return value:
  - If most callers check for null before use, it is likely intended to be
    `@Nullable`.
  - If most callers assume it is non-null (and would crash if null), consider
    keeping it non-null or refactoring to ensure it is non-null, rather than
    forcing all callers to handle null.

## Common Patterns & Recipes

### Recipe: Refactoring Receiver for Nullable Supplier

**Before (in Caller):**

```java
mReceiver = new Receiver(() -> assumeNonNull(nullableSupplier.get()));
```

**After:**

1. **In Receiver Class:**
   ```java
   // Change constructor to take exact Supplier<@Nullable Item>
   public Receiver(Supplier<@Nullable Item> supplier) {
       mSupplier = supplier;
   }

   // In usage (Dereferenced right away)
   void doSomething() {
       var item = mSupplier.get();
       assumeNonNull(item);
       item.use();
   }

   // In usage (Stored or Passed)
   void storeItem() {
       var item = mSupplier.get();
       assert item != null;
       mStoredItem = item;
   }
   ```
2. **In Caller:** If the caller has a `Supplier<DerivedItem>` and the receiver
   expects `Supplier<@Nullable BaseItem>`, use `SupplierUtils.upcast()` to pass
   it:
   ```java
   mReceiver = new Receiver(SupplierUtils.upcast(derivedSupplier, BaseItem.class));
   ```
   If the caller already has a `Supplier<@Nullable Item>`, pass it directly:
   ```java
   mReceiver = new Receiver(nullableSupplier);
   ```

### Note on Deep Investigation for Suppliers

Before applying the recipe above to force non-nullability or add assertions,
**investigate the receiver**. If the receiver (or classes it passes the supplier
to) already checks for null or can easily be updated to handle null, prefer
updating the signature to accept `Supplier<@Nullable T>` instead of forcing
non-nullability.

## Preferred Null Safety Patterns

- **ObservableSupplier / MonotonicObservableSupplier**: Prefer
  `supplier.asNonNull().get()` over `var x = supplier.get(); assert x != null;`.
- **Assertions and Chaining**: Use `assumeNonNull(object)` from
  `org.chromium.build.NullUtil` instead of `assert object != null` when you want
  to chain calls on the non-null object (e.g.,
  `assumeNonNull(mLayoutManager).getSomething()`). It returns the non-null
  object.
- **Asserting over Silent Checks**: If a code path guarantees that an object
  must be non-null, use an explicit assertion instead of a silent null check
  (e.g., changing `if (x != null)` to `assert x != null`).
- **Testing Getters**: `get_forTesting` methods should just return `@Nullable`
  (and be annotated as such) rather than asserting non-null, if the underlying
  field is nullable. Let the test handle the nullity.

## Testing

- **Smoke Test**: Use `PublicTransitLeakTest` as a smoke test locally before
  running all tests on CQ to validate functional changes introduced by
  assertions.

## Troubleshooting

- **Warning in Constructor**: If NullAway warns that a field is not initialized
  in the constructor, ensure it is marked `@Nullable` or `@MonotonicNonNull` if
  it's initialized later (e.g., in `init` or `initWithProfile`).
- **Method returns @Nullable but signature doesn't say so**: Add `@Nullable` to
  the method signature.
- **Satisfying Non-Null Callbacks**: Do NOT use `assumeNonNull(null)` to satisfy
  a callback that expects a non-null value if the value can actually be null.
  Update the callback definition to accept `@Nullable T`.
