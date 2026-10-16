---
name: general/place-feature-ui-strings-in-dedicated-grdp-files
rubric: design
severity: should_fix
---

**Rule.** Define UI strings in the .grd or .grdp resource file matching the UI
surface or feature displaying them (e.g., placing New Tab Page strings in
generated_resources.grd rather than glic_strings.grdp, or dedicated feature
strings in actor_strings.grdp), and use matching message ID prefixes (e.g.,
IDS_NTP\_\* or IDS_ACTOR\_\*). Ensure accompanying translation screenshots and
SHA1 hashes (.png.sha1) are saved under the matching <file>\_grd/ or
<file>\_grdp/ screenshot directory.

**Why.** GRIT resource files are partitioned by UI surface, feature, and
component OWNERS. Placing strings in an unrelated or adjacent feature partition
breaks ownership boundaries, violates naming consistency, and misplaces
translation screenshots in the wrong directory, complicating localization
reviews.

## Examples

Bad:

```cpp
<!-- In chrome/app/glic_strings.grdp -->
<message name="IDS_GLIC_ACTOR_OVERLAY_SNACKBAR_MESSAGE" desc="Message shown in snackbar when clicking actor overlay.">
  You cannot interact with this tab until the task is paused or complete
</message>
<message name="IDS_GLIC_DRIVE_CONNECT_TITLE" desc="Title of dialog asking to connect Google Drive on NTP.">
  Connect Google Drive?
</message>
```

Good:

```cpp
<!-- In chrome/app/actor_strings.grdp -->
<message name="IDS_ACTOR_OVERLAY_SNACKBAR_MESSAGE" desc="Message shown in snackbar when clicking actor overlay.">
  You cannot interact with this tab until the task is paused or complete
</message>

<!-- In chrome/app/generated_resources.grd -->
<message name="IDS_NTP_DRIVE_CONNECT_TITLE" desc="Title of dialog asking to connect Google Drive on NTP.">
  Connect Google Drive?
</message>
```
