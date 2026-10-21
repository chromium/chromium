---
name: chrome/download-interrupted-string-status-vs-description-naming
rubric: readability
severity: should_fix
---

**Rule.** When defining or consuming download interruption string resources
(IDS_DOWNLOAD_INTERRUPTED\_\*), name concise UI status labels with the _STATUS_
infix and full explanatory messages with _DESCRIPTION_. Do not name full
explanatory sentences with _STATUS_ or append ad-hoc UI suffixes (such as
\_DOWNLOAD_UI) to disambiguate messages.

**Why.** In download UI models (such as DownloadUIModel and OfflineItemUtils),
_STATUS_ strings map to compact failure state labels for shelves and
notifications, while _DESCRIPTION_ strings map to full failure explanations for
the downloads page. Consistent naming avoids confusion between status labels and
descriptions and eliminates ad-hoc disambiguation suffixes.

## Examples

Bad:

```cpp
<message name="IDS_DOWNLOAD_INTERRUPTED_STATUS_LOCAL_DOWNLOAD_BLOCKED"
         desc="The local download was blocked and redirected to cloud storage">
  Your organization blocked the local download of this file.
</message>
<message name="IDS_DOWNLOAD_INTERRUPTED_STATUS_LOCAL_DOWNLOAD_BLOCKED_DOWNLOAD_UI"
         desc="The local download was blocked and redirected to cloud storage due to security policy.">
  Local download blocked
</message>
```

Good:

```cpp
<message name="IDS_DOWNLOAD_INTERRUPTED_STATUS_LOCAL_DOWNLOAD_BLOCKED"
         desc="The local download was blocked and redirected to cloud storage due to security policy.">
  Local download blocked
</message>
<message name="IDS_DOWNLOAD_INTERRUPTED_DESCRIPTION_LOCAL_DOWNLOAD_BLOCKED"
         desc="The local download was blocked and redirected to cloud storage">
  Your organization blocked the local download of this file.
</message>
```
