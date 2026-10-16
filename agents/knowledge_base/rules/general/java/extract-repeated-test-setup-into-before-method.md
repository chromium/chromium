---
name: general/extract-repeated-test-setup-into-before-method
rubric: design
severity: should_fix
---

**Rule.** In unit tests, isolate common mock configurations, framework
initialization, and test environment setup into a `@Before` setup method (or
test fixture setup) rather than repeating identical setup boilerplate inline
across multiple test cases. Keep individual test methods focused strictly on
scenario-specific inputs, target interactions, and assertions.

**Why.** Repeating identical mock creation, stubbing, and context initialization
inside every test method creates visual clutter and increases maintenance churn
when setup requirements change. Centralizing shared setup in a dedicated setup
block ensures clean test separation, reduces code duplication, and keeps test
cases concise.

## Examples

Bad:

```cpp
@Test
public void testIsOpenableFile_ValidFile() {
    Context mockContext = Mockito.mock(Context.class);
    ContentResolver mockContentResolver = Mockito.mock(ContentResolver.class);
    when(mockContext.getContentResolver()).thenReturn(mockContentResolver);
    ContextUtils.initApplicationContextForTests(mockContext);
    // ...
}

@Test
public void testReadTextFromUri_MimeTypeMismatch() {
    Context mockContext = Mockito.mock(Context.class);
    ContentResolver mockContentResolver = Mockito.mock(ContentResolver.class);
    when(mockContext.getContentResolver()).thenReturn(mockContentResolver);
    ContextUtils.initApplicationContextForTests(mockContext);
    // ...
}
```

Good:

```cpp
@Mock private Context mMockContext;
@Mock private ContentResolver mMockContentResolver;

@Before
public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
    ContextUtils.initApplicationContextForTests(mMockContext);
}

@Test
public void testIsOpenableFile_ValidFile() {
    // ...
}

@Test
public void testReadTextFromUri_MimeTypeMismatch() {
    // ...
}
```
