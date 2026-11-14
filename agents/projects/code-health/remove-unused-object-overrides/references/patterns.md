# Implementation Patterns: Remove Unused Object Overrides

This reference details the rules, good/bad patterns, safety constraints, and
test-repair strategies when removing unreferenced `equals()`, `hashCode()`, and
`toString()` overrides in Chromium / Clank.

## Core Safety Rules

### Rule 1: Always Keep or Delete `equals()` and `hashCode()` Together

In Java, `equals()` and `hashCode()` are tied together by the Object contract.
Always keep or delete them as a pair:

- **Delete Both:** If neither `equals()` nor `hashCode()` is needed in
  production logic or asserted in unit tests, delete both `equals()` and
  `hashCode()`. Never delete just one.
- **Keep Both:** If production logic requires value equality (such as
  collections, state comparison, snapshot tokens) or if unit tests assert value
  equality (an `assertEquals` or Mockito verification comparing two different
  instances), keep both `equals()` and `hashCode()`. Do not delete them and
  substitute `refEq()` or custom field assertion helpers.

### Rule 2: Exhaustive Audit of All Indirect & Hashing Invocations

Before deleting any override, perform an exhaustive audit across the repository:

- **Global Symbol Search:** Search the repository for every reference to the
  class name to discover all usage sites.
- **Hashed Collections:** Is the class used as a key in
  `HashMap`/`LinkedHashMap`/`ConcurrentHashMap` or an element in
  `HashSet`/`LinkedHashSet`?
- **Direct & Indirect Hashing:** Are there any calls to `obj.hashCode()`,
  `Objects.hashCode(obj)`, `Arrays.hashCode(...)`, or `Objects.hash(..., obj)`?
- **Collection Lookups & Membership:** Is the class searched in collections via
  `List.contains(obj)`, `List.indexOf(obj)`, `List.lastIndexOf(obj)`,
  `List.remove(obj)`, or `Collection.removeAll(objs)`?
- **Direct & Indirect Equality:** Is the class compared via
  `Objects.equals(a, b)`, `ObjectsCompat.equals(a, b)`, or `a.equals(b)` in
  production?
- **Serialization & Reflection:** Is the class serialized via Proto, Mojo, JSON,
  Parcelable/AIDL, or consumed by reflection-based caching?
- **Public Component APIs:** Is the class exposed across component boundaries
  where external callers might rely on equality?
- **`toString()` Indirect Calls:** Is the class passed to string concatenation
  (`"" + obj`), `String.format("%s", obj)`, or logging (`Log.d/i/e`)?

## Refactoring Patterns

### Pattern 1: Dead `equals()` and `hashCode()` on UI Property Holders

#### Bad Pattern

```java
public class TabActionButtonData {
    public final int type;
    public final TabActionListener tabActionListener;

    public TabActionButtonData(int type, TabActionListener tabActionListener) {
        this.type = type;
        this.tabActionListener = tabActionListener;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof TabActionButtonData other)) return false;
        return type == other.type && Objects.equals(tabActionListener, other.tabActionListener);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.type, this.tabActionListener);
    }
}
```

#### Good Pattern

```java
public class TabActionButtonData {
    public final int type;
    public final TabActionListener tabActionListener;

    public TabActionButtonData(int type, TabActionListener tabActionListener) {
        this.type = type;
        this.tabActionListener = tabActionListener;
    }
}
```

### Pattern 2: Test-Only `equals()` Verification with Mockito

When a method is called in production with a newly allocated object and verified
in unit tests, the Mockito verification is a caller that requires value
equality. A broken `verify(...)` after removing `equals()` means callers need
it, so keep both `equals()` and `hashCode()`. Do **not** delete them and
substitute `refEq()` or custom field assertion helpers; that is prohibited.

#### Bad Pattern (Prohibited)

```java
// Production: equals() and hashCode() deleted from PriceTabData.
static class PriceTabData {
    public final int bindingTabId;
    public final PriceDrop priceDrop;

    PriceTabData(int bindingTabId, PriceDrop priceDrop) { ... }
}

// Unit test rewritten with refEq() as a workaround:
import static org.mockito.ArgumentMatchers.refEq;

verify(mController).showPriceWelcomeMessage(refEq(mPriceTabData));
```

#### Good Pattern

```java
// Production class keeps BOTH equals() and hashCode():
static class PriceTabData {
    public final int bindingTabId;
    public final PriceDrop priceDrop;

    PriceTabData(int bindingTabId, PriceDrop priceDrop) { ... }

    @Override
    public boolean equals(Object obj) { ... }

    @Override
    public int hashCode() { ... }
}

// Unit test is unchanged:
verify(mController).showPriceWelcomeMessage(mPriceTabData);
```

### Pattern 3: Test-Only `equals()` with JUnit `assertEquals()`

When unit tests compare objects with `assertEquals(expected, actual)` and the
two are different instances, the test is a caller that requires value equality.
Keep both `equals()` and `hashCode()`. Do **not** delete them and replace the
assertion with a custom field assertion helper; that is prohibited.

#### Bad Pattern (Prohibited)

```java
// Production: equals() and hashCode() deleted from TitleData.

// In Test: custom field assertion helper written as a workaround.
private static void assertTitleData(TitleData expected, TitleData actual) {
    assertNotNull("Expected non-null TitleData", actual);
    assertEquals(expected.title, actual.title);
    assertEquals(expected.numTabs, actual.numTabs);
    assertEquals(expected.rowAccessibilityTextResId, actual.rowAccessibilityTextResId);
}
assertTitleData(new TitleData("Bar", 3, R.plurals.text), model.get(TITLE_DATA));
```

#### Good Pattern

```java
// Production: keep both equals() and hashCode() on TitleData.
// In Test:
assertEquals(new TitleData("Bar", 3, R.plurals.text), model.get(TITLE_DATA));
```

### Pattern 4: Dead `toString()` Overrides

#### Bad Pattern

```java
public class LayoutTab {
    private final int mId;

    @Override
    public String toString() {
        return Integer.toString(mId);
    }
}
```

#### Good Pattern

Delete `toString()` once the usage checklist finds no caller. IDEs and debuggers
inspect `mId` directly.
