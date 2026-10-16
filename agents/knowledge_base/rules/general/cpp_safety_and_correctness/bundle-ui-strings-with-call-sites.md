---
name: general/bundle-ui-strings-with-call-sites
rubric: correctness
severity: should_fix
---

**Rule.** Bundle newly added or updated UI string resources (IDS\_\*) with their
corresponding UI call sites and consuming code in the same CL rather than
landing string definitions in a separate precursor CL. If strings are
intentionally landed in advance of their implementation, explicitly clarify in
the commit description that the CL only adds resource definitions.

**Why.** Splitting string definitions from their consuming call sites leaves
unused resources, indicates that implementation files may have been accidentally
omitted during upload, and risks landing orphaned strings or creating
cherry-pick hazards on release branches. Bundling string declarations with their
usage ensures atomic changes and explicit release-branch mergeability.

## Examples

Bad:

```cpp
<!-- In chrome/app/generated_resources.grd without including caller files in the CL -->
<message name="IDS_READING_MODE_TOOLTIP" desc="Tooltip for reading mode">
  Reading mode (Alt+Shift+R)
</message>
<!-- Consuming UI call sites are omitted without commit message clarification -->
```

Good:

```cpp
<!-- In the same CL: -->
<!-- 1. chrome/app/generated_resources.grd -->
<message name="IDS_READING_MODE_TOOLTIP" desc="Tooltip for reading mode">
  Reading mode (Alt+Shift+R)
</message>

// 2. chrome/browser/ui/browser_actions.cc
root_action_item_->AddChild(
    SidePanelAction(SidePanelEntryId::kReadAnything, IDS_READING_MODE_TITLE,
                    IDS_READING_MODE_TOOLTIP, kMenuBookChromeRefreshIcon,
                    kActionSidePanelShowReadAnything, bwi, true)
        .Build());
```
