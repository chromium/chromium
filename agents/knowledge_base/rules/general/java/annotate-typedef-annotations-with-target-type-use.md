---
name: general/annotate-typedef-annotations-with-target-type-use
rubric: readability
severity: optional
---

**Rule.** When declaring Java typedef annotations (`@IntDef`, `@LongDef`, or
`@StringDef`), annotate the `@interface` declaration with
`@Target(ElementType.TYPE_USE)` alongside `@Retention(RetentionPolicy.SOURCE)`.
This ensures the typedef annotation can qualify primitive or boxed types across
variable declarations, method return types, casts, and generic type arguments.

**Why.** In Java, annotations without an explicit `@Target` apply only to
declaration elements by default and cannot be used in type-use contexts such as
type casts, generic type arguments, or return type qualifiers. Adding
`@Target(ElementType.TYPE_USE)` ensures the typedef annotation can be applied
wherever the underlying primitive or boxed type is used.

## Examples

Bad:

```cpp
@IntDef({
    ServiceNames.JAVA_SANDBOXED,
    ServiceNames.NATIVE_ONLY_SANDBOXED,
    ServiceNames.PRIVILEGED,
    ServiceNames.OTHER
})
@Retention(RetentionPolicy.SOURCE)
public @interface ServiceNames {
    int JAVA_SANDBOXED = 0;
    int NATIVE_ONLY_SANDBOXED = 1;
    int PRIVILEGED = 2;
    int OTHER = 3;
}
```

Good:

```cpp
@IntDef({
    ServiceNames.JAVA_SANDBOXED,
    ServiceNames.NATIVE_ONLY_SANDBOXED,
    ServiceNames.PRIVILEGED,
    ServiceNames.OTHER
})
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
public @interface ServiceNames {
    int JAVA_SANDBOXED = 0;
    int NATIVE_ONLY_SANDBOXED = 1;
    int PRIVILEGED = 2;
    int OTHER = 3;
}
```
