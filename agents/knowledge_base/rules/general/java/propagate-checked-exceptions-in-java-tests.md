---
name: general/propagate-checked-exceptions-in-java-tests
rubric: readability
severity: optional
---

**Rule.** In Java test classes, @Test methods, fixture lifecycle hooks (@Before,
@After), and test helper methods, declare checked exceptions (such as throws
Exception or throws IOException) directly on the method signature. Do not catch
checked exceptions solely to rethrow or wrap them in RuntimeException (or
JavaUtils.throwUnchecked), and do not swallow them.

**Why.** Test runners (such as JUnit and Robolectric) automatically catch
unhandled exceptions thrown by test methods and helper calls, failing the test
and displaying complete stack traces. Wrapping checked exceptions in
RuntimeException or helper utilities adds verbose try-catch boilerplate and
clutters failure reports with redundant nested exception causes.

## Examples

Bad:

```cpp
private String listAllPaths(Path rootDir) {
    try {
        Files.walkFileTree(rootDir, new SimpleFileVisitor<Path>() { ... });
    } catch (IOException e) {
        throw new RuntimeException(e);
    }
}

@Test
public void testFileOperations() {
    try {
        performOperation();
    } catch (Exception e) {
        throw JavaUtils.throwUnchecked(e);
    }
}
```

Good:

```cpp
private String listAllPaths(Path rootDir) throws IOException {
    Files.walkFileTree(rootDir, new SimpleFileVisitor<Path>() { ... });
}

@Test
public void testFileOperations() throws Exception {
    performOperation();
}
```
