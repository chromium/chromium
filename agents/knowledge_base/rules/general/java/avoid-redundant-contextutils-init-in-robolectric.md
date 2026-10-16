---
name: general/avoid-redundant-contextutils-init-in-robolectric
rubric: readability
severity: nit
---

**Rule.** In Android Robolectric JUnit tests running under
BaseRobolectricTestRunner, do not manually call
ContextUtils.initApplicationContextForTests(...) with the default application
context (such as RuntimeEnvironment.application or
ApplicationProvider.getApplicationContext()). Access the application context
directly via ContextUtils.getApplicationContext() or
ApplicationProvider.getApplicationContext(), reserving
initApplicationContextForTests strictly for custom mock contexts or instrumented
test runners.

**Why.** BaseRobolectricTestRunner automatically initializes ContextUtils with
ApplicationProvider.getApplicationContext() via BaseRobolectricTestRule.setUp()
before each test runs. Manually re-initializing ContextUtils in @Before setup
methods introduces redundant boilerplate.

## Examples

Bad:

```cpp
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenOffBroadcastReceiverTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        ContextUtils.initApplicationContextForTests(mContext);
    }
}
```

Good:

```cpp
@RunWith(BaseRobolectricTestRunner.class)
public class ScreenOffBroadcastReceiverTest {
    @Before
    public void setUp() {
        // ContextUtils is already initialized by BaseRobolectricTestRunner.
    }

    private void sendIntent(Intent intent) {
        ContextUtils.getApplicationContext().sendBroadcast(intent);
    }
}
```
