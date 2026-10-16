---
name: general/avoid-mocking-sendbroadcast-in-robolectric
rubric: correctness
severity: should_fix
---

**Rule.** In Android Robolectric JUnit tests, do not spy on Context or use
verify() in @Before setup to assert static singleton initialization (such as
registerReceiver()), as singletons persist across tests and will not
re-initialize if already instantiated by a previous test. Additionally, do not
mock Context.sendBroadcast() to manually invoke BroadcastReceiver.onReceive() on
captured receivers. Use real Context.sendBroadcast() calls and drain the main
looper tasks (e.g., via
Shadows.shadowOf(Looper.getMainLooper()).runToEndOfTasks() or
BaseRobolectricTestRule.runAllBackgroundAndUi()) to test broadcast handling
natively.

**Why.** Static singletons retain state across tests in the same process,
causing setup verification to fail flakily whenever another test has already
instantiated the singleton. Furthermore, Robolectric natively simulates
Android's broadcast subsystem and intent filtering; bypassing real broadcast
dispatch with mocks and spies adds brittle boilerplate and skips framework
validation.

## Examples

Bad:

```cpp
// Anti-pattern 1: Spying on Context in setup to assert singleton registration
@Before
public void setUp() {
    mContext = spy(RuntimeEnvironment.application);
    ContextUtils.initApplicationContextForTests(mContext);
    ScreenOffBroadcastReceiver.getInstance();
    verify(mContext).registerReceiver(mReceiverCaptor.capture(), any(), any(), anyInt());
}

// Anti-pattern 2: Mocking sendBroadcast to manually invoke onReceive()
doAnswer(invocation -> {
    Intent intent = invocation.getArgument(0, Intent.class);
    mReceiverCaptor.getValue().onReceive(mContext, intent);
    return null;
}).when(mContext).sendBroadcast(any(Intent.class));
```

Good:

```cpp
// Setup: Initialize context without spies; do not verify singleton initialization
@Before
public void setUp() {
    mContext = RuntimeEnvironment.application;
    ContextUtils.initApplicationContextForTests(mContext);
    ScreenOffBroadcastReceiver.getInstance();
    BaseRobolectricTestRule.runAllBackgroundAndUi();
}

// Test: Use real sendBroadcast and drain the main looper tasks
private void sendIntent(Intent intent) {
    mContext.sendBroadcast(intent);
    Shadows.shadowOf(Looper.getMainLooper()).runToEndOfTasks();
}
```
