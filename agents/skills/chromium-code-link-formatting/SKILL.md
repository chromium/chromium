---
name: chromium-code-link-formatting
description: >-
  Use this skill when you need to link to files or specific lines in the Chromium codebase within a document intended for external sharing (e.g., Google Docs, Buganizer). This skill ensures links use the stable `source.chromium.org` format with line numbers and commit hashes for accessibility, even if the user doesn't explicitly specify the format. Do not use for internal or purely local notes where file:/// links are preferred.
---

# Chromium Source Link Formatter

## Instructions

When formatting links to the Chromium codebase for documents that will be shared
externally:

1. **Use the following URL format:**
   `https://source.chromium.org/chromium/chromium/src/+/main:{path_to_file};l={line_no};drc={commit_hash}`

2. **Resolve Placeholders:**

   - `path_to_file`: The path to the file relative to the repository root.
   - `line_no`:
     - Single line: `l=20`
     - Range: `l=400-500` (inclusive)
     - Multiple ranges: `l=20,400-500`
     - Whole file: Leave empty (`l=`)
   - `commit_hash`:
     - If the user specifies a hash, use it.
     - Otherwise, execute `git merge-base HEAD origin/main` to find the latest
       common ancestor and use that hash.
