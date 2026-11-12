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
The discovery plugin returns candidate file paths, not individual line
numbers or interface types. You must locate each anonymous inner class inside
each file yourself.

[INSERT LIST OF CANDIDATE FILES HERE, e.g.:
- chrome/browser/init/ProcessInitializationHandler.java
- chrome/browser/init/LaunchFailedActivity.java
]

### Verification Instructions:
For each candidate file:
1.  **Locate Anonymous Classes:** Use `rg` (e.g., `rg -n "new [A-Z][A-Za-z0-9_.<>]*\(.*\) \{" <file>`) and read the file to find every anonymous inner class. Treat each anonymous class as its own candidate.
2.  **Locate the Interface Definition:**
    *   If it is a custom Chromium interface (e.g., starts with `org.chromium.`), use `rg` or file search tools to find its definition file (`.java`). Read the file.
    *   If it is a standard Java/Android SDK interface (e.g., `android.content.DialogInterface.OnClickListener`), use your pre-trained knowledge.
3.  **Verify SAM (Single Abstract Method) Status:**
    *   Confirm the target is an `interface` (not an `abstract class` like `AsyncTask` or `TransitionListener`).
    *   Confirm it declares exactly **one** abstract method (functional interface).
4.  **Check for Lambda Compatibility (reject if any applies):**
    *   **Annotations:** If the overridden method has annotations like `@JavascriptInterface`, `@SuppressLint`, or test annotations, it **cannot** be converted to a lambda. Reject it. Do not suggest moving `@SuppressLint` to the field or enclosing method.
    *   **`this` usage:** If the body uses `this` to refer to the anonymous class instance itself, it **cannot** be converted to a lambda. Reject it.
    *   **Blank final fields:** If the anonymous class is in a field initializer and references blank final fields initialized in the constructor, converting it causes a compile error. Reject it.
    *   **`View.OnTouchListener`:** Reject it even though it is a SAM interface; it often triggers `ClickableViewAccessibility` lint warnings and can break lint baselines.

### Output Format:
Provide a final summary in markdown table format:

| File | Line | Interface Type | Status | Reason |
| :--- | :--- | :--- | :--- | :--- |
| `ProcessInitializationHandler.java` | 123 | `ChildCrashedCallback` | VERIFIED | SAM interface, no annotations, no `this` usage. |
| `SomeActivity.java` | 45 | `AsyncTask` | REJECTED | Abstract class, not an interface. |
| `OtherActivity.java` | 60 | `Runnable` | REJECTED | Uses `this` to remove callback. |
```
