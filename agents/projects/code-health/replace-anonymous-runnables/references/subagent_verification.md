# Subagent Verification: SAM Interfaces

When auditing a subsystem for anonymous-class-to-lambda conversions, the raw
scanner (`find_candidates.py`) may flag false positives (such as abstract
classes or interfaces with multiple methods).

To verify the candidates reliably, spawn a `self` subagent with the following
prompt to filter the raw list.

______________________________________________________________________

## Subagent Prompt Template

```markdown
You are a specialized Code Health Verifier. Your task is to audit a list of potential anonymous-class-to-lambda candidates and filter out false positives.

### Input Candidates:
[INSERT LIST OF CANDIDATES HERE, e.g.:
- org.chromium.chrome.browser.init.ProcessInitializationHandler: L123 (ChildProcessCrashObserver.ChildCrashedCallback)
- org.chromium.chrome.browser.init.LaunchFailedActivity: L45 (DialogInterface.OnClickListener)
]

### Verification Instructions:
For each candidate:
1.  **Locate the Interface Definition:**
    *   If it is a custom Chromium interface (e.g., starts with `org.chromium.`), use `code_search` or `find_by_name` to find its definition file (`.java`). Read the file.
    *   If it is a standard Java/Android SDK interface (e.g., `android.content.DialogInterface.OnClickListener`), use your pre-trained knowledge.
2.  **Verify SAM (Single Abstract Method) Status:**
    *   Confirm the target is an `interface` (not an `abstract class` like `AsyncTask` or `TransitionListener`).
    *   Confirm it declares exactly **one** abstract method (functional interface).
3.  **Check for Lambda Compatibility:**
    *   View the candidate file at the specified line number.
    *   Check the anonymous class implementation. If the overridden method has annotations like `@JavascriptInterface` or test annotations, it **cannot** be converted to a lambda. Skip it.
    *   Check if the body uses `this` to refer to the anonymous class instance itself. If so, it **cannot** be converted to a lambda. Skip it.

### Output Format:
Provide a final summary in markdown table format:

| File | Line | Interface Type | Status | Reason |
| :--- | :--- | :--- | :--- | :--- |
| `ProcessInitializationHandler.java` | 123 | `ChildCrashedCallback` | VERIFIED | SAM interface, no annotations, no `this` usage. |
| `SomeActivity.java` | 45 | `AsyncTask` | REJECTED | Abstract class, not an interface. |
| `OtherActivity.java` | 60 | `Runnable` | REJECTED | Uses `this` to remove callback. |
```
