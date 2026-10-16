---
name: general/use-direct-action-verbs-for-toggle-accessibility-labels
rubric: testing
severity: should_fix
---

**Rule.** Phrase toggle button accessibility labels and string resources with
direct action verbs (e.g., 'Pin <feature>' and 'Unpin <feature>') rather than
indirect capability phrasing like 'Enable pin ...' or 'Disable pin ...'. When a
toggle control has an active or toggled state, ensure the accessible label for
toggling off conveys the corresponding reverse action.

**Why.** Activating a toggle directly performs the action rather than enabling
or disabling the capability to perform it. Using concise direct action verbs
ensures assistive technologies announce the control's immediate effect and state
transitions clearly to users.

## Examples

Bad:

```cpp
<message name="IDS_READING_MODE_DISABLE_PIN_BUTTON_LABEL" desc="...">
  Enable pin reading mode
</message>
<message name="IDS_READING_MODE_ENABLE_PIN_BUTTON_LABEL" desc="...">
  Disable pin reading mode
</message>
```

Good:

```cpp
<message name="IDS_READING_MODE_DISABLE_PIN_BUTTON_LABEL" desc="...">
  Pin reading mode
</message>
<message name="IDS_READING_MODE_ENABLE_PIN_BUTTON_LABEL" desc="...">
  Unpin reading mode
</message>
```
