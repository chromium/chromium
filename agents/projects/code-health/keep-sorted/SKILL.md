---
name: keep-sorted
description: Automatically detects misordered lists in code (like Android XMLs), injects go/keep-sorted tags, sorts them, and creates CLs.
---

# 🧹 Keep-Sorted Finder & Fixer

This skill empowers AI agents to proactively maintain Chromium's code health by
identifying lists that should be alphabetically sorted, injecting
`go/keep-sorted` presubmit directives, and automatically fixing the order of
elements.

## 🎯 When to Use This Skill

- **Code Reviews:** When you notice unsorted lists during a CL review.
- **Code Health Audits:** When you are proactively looking for technical debt or
  running a cleanup sidecar.
- **Explicit Requests:** When the user explicitly requests to "find keep-sorted
  issues" or clean up resource files.

## 🛠️ Step-by-Step Instructions

### 1. Identify Target Files

You should scan for files that conventionally require sorted lists but are
missing the block tags. Use tools like `grep_search` or `find_by_name`. **Common
Target Types:**

- `strings.xml` (e.g., `android/java/res/values/strings.xml`)
- `colors.xml`
- `dimens.xml`
- `ids.xml`
- `BUILD.gn` (for dependency lists)

### 2. Detect Block Violations

Look for contiguous blocks of similar elements (like `<string name="...">`,
`<dimen name="...">`, `<color name="...">` or `deps = [ ... ]`) that are
naturally suited for alphabetical sorting. Pay close attention to blocks that
lack the `<!-- go/keep-sorted` `start -->` and `<!-- go/keep-sorted` `end -->`
tags.

### Pattern Examples

**Bad (Unsorted, missing tags):**

```xml
<dimen name="margin_top">16dp</dimen>
<dimen name="margin_bottom">8dp</dimen>
```

**Good (Sorted, with tags):**

```xml
<!-- go/keep-sorted start -->
<dimen name="margin_bottom">8dp</dimen>
<dimen name="margin_top">16dp</dimen>
<!-- go/keep-sorted end -->
```

### 3. Inject Directives & Sort

1. **Inject Tags:** Manually inject the `go/keep-sorted` `start` tag before the
   first item and the `go/keep-sorted` `end` tag after the last item. Use the
   appropriate comment syntax for the file type (e.g., `<!-- -->` for XML, `#`
   for GN).
2. **Alphabetize:** Sort the block alphabetically. If available, you can run the
   local `keep-sorted` formatter over the file; otherwise, sort the items
   manually in your script or output.

### 4. Create Tracking Bugs

Every cleanup CL must be associated with a tracking bug.

- **If the user provides a bug ID:** Use that bug ID in the CL description.
- **If NO bug ID is provided:** You MUST first file a new child bug under the
  parent tracker **Bug 544831881** (`[Code Health] Maintain keep-sorted`). Use
  this newly created child bug's ID for your CL.

### 5. Upload Code Health CL

Create a new branch and upload your changes to Gerrit. Your CL commit message
must include the `[Code Health]` tag and reference the tracking bug:

```
[Code Health] Add keep-sorted for <file_context>

Bug: <BUG_ID>
```
