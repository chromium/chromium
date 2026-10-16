---
name: general/omit-scratch-and-generator-output-files
rubric: readability
severity: should_fix
---

**Rule.** Keep change lists focused strictly on their intended change; do not
stage or commit scratch files, obsolete placeholder or stub files with comments
deferring their removal, local generator outputs (such as stray generated
headers at repository roots), build artifacts or tool logs (such as
siso_result.json), branch configuration artifacts (such as
chrome_branch_deps.json), accidental repository configuration edits (such as
.gitmodules or personal .gitignore rules), rebase or merge artifacts (such as
extraneous entries picked up during upstream synchronization in AUTHORS), or
files containing only whitespace, formatting, or indentation churn. Delete or
omit unneeded files directly rather than deferring their removal, revert
untouched files and accidental modifications without substantive changes, manage
personal workspace exclusions in .git/info/exclude, and perform a self-review of
uploaded files and diffs in Gerrit before requesting review.

**Why.** Accidentally committing local generator outputs, scratch files,
obsolete stubs, tool logs, build artifacts, branch release metadata, rebase
artifacts, personal environment settings, or whitespace-only edits litters the
repository with unmaintained artifacts, clutters code reviews with noise, risks
violating directory OWNERS policies, muddles Git blame history, and risks
introducing unintended build or configuration side-effects.

## Examples

Bad:

```cpp
// Staging an obsolete stub with deferred removal comments:
// base/feature_rust_shim.h:
// This file is no longer needed and can be removed in a separate CL.

// Staging a stray scratch or generator output file at repository root:
// out.h:
// THIS FILE IS GENERATED FROM ai_overlay_dialog_page_handler.h. DO NOT EDIT.
#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOL_DEFINITIONS_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOL_DEFINITIONS_H_

// Staging local build artifacts or tool logs:
// siso_result.json:
{
  "code": 1,
  "message": "Error: build.ninja not found. need `-C <dir>`?"
}

// Committing personal editor/agent preferences in repository .gitignore:
// .gitignore:
+/.github/skills/personal-preferences/

// Accidental whitespace/indentation-only changes in untouched files:
// chrome/app/glic_strings.grdp:
-    Current toolbar shortcut
+      Current toolbar shortcut
```

Good:

```cpp
// Delete obsolete stubs and omit unneeded scratch/log/metadata files completely:
$ git rm base/feature_rust_shim.h base/feature_rust_shim.cc out.h siso_result.json
$ git checkout -- .gitmodules .gitignore chrome/app/glic_strings.grdp

// Keep only the author's intended entry when updating metadata files:
// AUTHORS:
+Hafiizh <sas.kunz@gmail.com>

// Manage personal editor and agent exclusions locally instead:
$ echo "/.github/skills/personal-preferences/" >> .git/info/exclude

// In Gerrit, perform a self-review ("Expand All") to verify that only
// intended files and line-by-line changes are included in the patchset.
```
