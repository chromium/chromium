---
name: cl-link-from-hash
description: >-
  Use this skill to get a link to a Gerrit Changelist (CL) from a given commit hash in the Chromium repository. Use when you need to find the review discussion, comments, or related issues for a specific commit. Don't use for linking to files or code lines.
---

# Getting CL Link from Commit Hash

## Method 1: Using `git log` (Fastest for landed commits)

If the commit has been merged into the main repository, the commit message
usually contains a `Reviewed-on:` field with the direct link to the CL.

1. Run the following command:
   ```bash
   git log -1 <commit_hash>
   ```
2. Look for the `Reviewed-on:` line in the output. Example:
   ```
   Reviewed-on: https://chromium-review.googlesource.com/c/chromium/src/+/8046200
   ```

## Method 2: Searching Gerrit by Commit Hash

If the commit message doesn't contain `Reviewed-on:` or you want to search
online, you can use the commit hash in a Gerrit search query.

Construct the search URL:
`https://chromium-review.googlesource.com/q/commit:<commit_hash>`

Example:
`https://chromium-review.googlesource.com/q/commit:87f30e397138c01de5b0d1fb7d4d8c5dc465f4ee`

## Method 3: Searching Gerrit by Change-Id

If you have the `Change-Id` (often found in the commit message as
`Change-Id: I...`), you can search Gerrit with it.

Construct the search URL:
`https://chromium-review.googlesource.com/q/<change_id>`

Example:
`https://chromium-review.googlesource.com/q/I8766a8ca89f5751a4c05f5a2430c1dc8b00d0d11`
