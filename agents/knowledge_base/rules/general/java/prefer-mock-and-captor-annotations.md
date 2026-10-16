---
name: general/prefer-mock-and-captor-annotations
rubric: readability
severity: should_fix
---

**Rule.** In Java tests, declare mocks, spies, and argument captors as `@Mock`,
`@Spy`, or `@Captor` class fields rather than instantiating them inline via
`Mockito.mock(...)`, `spy(...)`, or `ArgumentCaptor.forClass(...)`. Reserve
inline creation strictly for cases where a dynamic or variable number of test
doubles is required at runtime.

**Why.** Class field annotations make test doubles and captors immediately
visible at the fixture level, promote instance reuse across test cases, reduce
GC overhead, and align with Chromium Java style conventions.

## Examples

Bad:

```cpp
@Test
public void testIsOpenableFile_ValidFile() {
    Context mockContext = Mockito.mock(Context.class);
    ContentResolver mockContentResolver = Mockito.mock(ContentResolver.class);
    ArgumentCaptor<Uri> uriCaptor = ArgumentCaptor.forClass(Uri.class);

    when(mockContext.getContentResolver()).thenReturn(mockContentResolver);
    // ...
}
```

Good:

```cpp
@Mock private Context mMockContext;
@Mock private ContentResolver mMockContentResolver;
@Captor private ArgumentCaptor<Uri> mUriCaptor;

@Before
public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mMockContext.getContentResolver()).thenReturn(mMockContentResolver);
}

@Test
public void testIsOpenableFile_ValidFile() {
    // ...
}
```
