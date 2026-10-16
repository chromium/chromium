---
name: general/split-grd-strings-and-screenshots-into-separate-cl
rubric: change_scope
severity: nit
---

**Rule.** When adding or modifying translatable UI strings in `.grd` or `.grdp`
resource files that require translation screenshots (`upload_screenshots.py`),
split the string definitions and associated screenshot hash files (`.png.sha1`)
into a dedicated precursor CL separate from functional code and feature logic.

**Why.** Isolating string and screenshot changes in a dedicated precursor CL
separates translation resource approvals and screenshot artifacts from business
logic reviews, keeps CL scope focused, and unblocks translation pipelines early
without stalling on code review iterations.

## Examples

Bad:

```cpp
<!-- In a single combined CL modifying feature logic and resource strings: -->
<!-- chrome/app/generated_resources.grd -->
<message name="IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_CANVAS" desc="Placeholder text for search box">
  Create anything
</message>
<!-- chrome/browser/ui/webui/searchbox_handler.cc -->
void SearchboxHandler::HandleSearch() { ... }
```

Good:

```cpp
<!-- In a dedicated precursor CL adding only string resources and screenshot hashes: -->
<!-- chrome/app/generated_resources.grd -->
<message name="IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_CANVAS" desc="Placeholder text for search box">
  Create anything
</message>
<!-- chrome/app/generated_resources_grd/IDS_NTP_SEARCH_BOX_DYNAMIC_PLACEHOLDER_CANVAS.png.sha1 -->
41a094999746cd906300334be450e34108a82dec
```
