---
name: general/statically-import-java-test-assertions
rubric: readability
severity: should_fix
---

**Rule.** In Java test files, statically import individual assertion and mocking
methods from `org.junit.Assert` (such as `assertEquals`, `assertTrue`,
`assertFalse`, `fail`), `org.mockito.Mockito` (such as `mock`, `spy`, `verify`,
`when`, `doReturn`), and `org.mockito.ArgumentMatchers` (such as `eq`, `any`)
rather than qualifying calls with class names (such as
`Assert.assertEquals(...)` or `Mockito.mock(...)`). Note that class-level
`@Mock` fields are preferred over inline `mock()` calls when mocks are shared
across test cases.

**Why.** Static imports eliminate repetitive class-qualifier boilerplate,
keeping test assertions and mocking statements concise, fluent, and readable.
Standard JUnit and Mockito test methods are unambiguous and universally
recognized in test classes, so qualifying them with class names adds unnecessary
visual noise.

## Examples

Bad:

```cpp
import org.junit.Assert;
import org.mockito.Mockito;

@Test
public void testProcessFile() {
    AssetFileDescriptor afd = Mockito.mock(AssetFileDescriptor.class);
    Mockito.verify(mObserver).onSuccess();
    Assert.assertEquals("File count mismatch", 1, mCounter.get());
    Assert.fail("Operation should have thrown an exception");
}
```

Good:

```cpp
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@Test
public void testProcessFile() {
    AssetFileDescriptor afd = mock(AssetFileDescriptor.class);
    verify(mObserver).onSuccess();
    assertEquals("File count mismatch", 1, mCounter.get());
    fail("Operation should have thrown an exception");
}
```
