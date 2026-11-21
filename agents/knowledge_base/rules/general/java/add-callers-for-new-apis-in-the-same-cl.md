---
name: general/add-callers-for-new-apis-in-the-same-cl
rubric: design
severity: should_fix
---

**Rule.** A new public method, helper, overload, `*ForTesting()` setter or
output field must have at least one caller or reader in the same CL. Ask the
author to fold in the first real use, or to drop the addition until it is
needed. A shared helper that lands first with its own unit tests, or one whose
commit message names the follow-up CL that uses it, is fine.

**Why.** Without a caller, neither the CQ nor the reviewer can tell whether the
API's shape fits its real use, and additions made "for a follow-up" often stay
dead when the follow-up changes course.

## Examples

Bad:

```java
// FooMediator.java: added in this CL; nothing in the CL calls it.
public void setPrefServiceForTesting(PrefService prefService) {
    mPrefService = prefService;
}
```

Good:

```java
// FooMediatorTest.java, in the same CL:
mMediator.setPrefServiceForTesting(mFakePrefService);
assertTrue(mMediator.isEnabled());
```
