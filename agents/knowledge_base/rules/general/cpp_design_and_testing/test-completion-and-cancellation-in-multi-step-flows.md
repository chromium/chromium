---
name: general/test-completion-and-cancellation-in-multi-step-flows
rubric: testing
severity: should_fix
---

**Rule.** When implementing multi-step user flows, wizard sequences, or chained
UI dialogs (such as authentication followed by permission confirmation), include
test coverage for both the sequential completion path and intermediate
cancellation or failure states. Verify that rejecting or dismissing an
intermediate step suppresses subsequent dialogs, cleans up pending operations,
and invokes cancellation or failure callbacks.

**Why.** Testing only the happy path leaves cancellation and failure paths
unverified, risking regressions where downstream dialogs appear unexpectedly,
unauthorized operations proceed despite rejection, or callbacks never execute.

## Examples

Bad:

```cpp
// Testing only the happy path of a multi-step dialog/permission sequence.
IN_PROC_BROWSER_TEST_F(SupervisedUserExtensionWebstorePrivateApiTestAndroid,
                       ParentApprovalDialogShownAndAccepted) {
  set_dialog_actions(DialogActions::kFullInstall);
  ASSERT_TRUE(RunInstallTest("install_child.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_success());
}
```

Good:

```cpp
// Test early cancellation to ensure subsequent dialogs are suppressed and failure callback fires.
IN_PROC_BROWSER_TEST_F(
    SupervisedUserExtensionWebstorePrivateApiTestAndroid,
    ParentApprovalInstallDialogNotShownOnParentAuthenticationCancel) {
  set_dialog_actions(DialogActions::kDismissParentAuthenticationDialog);
  ASSERT_TRUE(RunInstallTest("install_cancel_child.html", "app.crx"));
  listener.Wait();
  ASSERT_TRUE(listener.received_failure());
  EXPECT_NE(ExtensionInstallPrompt::g_last_prompt_type_for_tests,
            ExtensionInstallPrompt::EXTENSION_PARENT_APPROVAL_PROMPT);
}

// Also test cancellation at intermediate stages as well as full sequential completion.
```
