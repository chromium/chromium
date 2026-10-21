---
name: chrome/no-legacy-idc-command-ids-for-new-actions
rubric: design
severity: should_fix
---

**Rule.** When declaring new Chrome actions in chrome_action_id.h, do not
introduce a corresponding legacy IDC\_\* command ID in chrome_command_ids.h.
Register new actions using the single-argument E(kActionName) macro unless
mapping an existing legacy command ID.

**Why.** Command IDs (IDC\_*) belong to Chrome's legacy command routing system
that is being migrated to the Actions framework. The optional second parameter
in E(...) exists only for documenting correspondence with existing legacy
command IDs; new actions do not require and should not introduce new IDC\_*
entries.

## Examples

Bad:

```cpp
// In chrome/app/chrome_command_ids.h:
#define IDC_INDIGO 40303

// In chrome/browser/ui/actions/chrome_action_id.h:
E(kActionIndigo, IDC_INDIGO)
```

Good:

```cpp
// In chrome/browser/ui/actions/chrome_action_id.h:
E(kActionIndigo)
```
