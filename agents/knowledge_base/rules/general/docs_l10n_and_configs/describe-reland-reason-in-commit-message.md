---
name: general/describe-reland-reason-in-commit-message
rubric: readability
severity: should_fix
---

**Rule.** In the commit message of a reland CL, specifically explain what was
fixed or changed to resolve the revert reason and make relanding safe
(identifying which patch set contains the fix diff), including detailing any
significant modifications such as renamed features, flags, or field trials,
rather than using vague summaries like 'fixed'. When creating a reland by
reverting a revert CL, replace the auto-generated 'Reason for revert:' template
heading with 'Reason for reland:'. If no code modifications were needed (e.g.,
false culprit diagnosis or resolved in a preceding CL), explain why and link to
any preceding CL.

**Why.** Reviewers, sheriffs, and tree gardeners need clear context to verify
why the revert occurred, understand all behavioral and naming changes, and
inspect just the diff of the fix against the original change. Leaving 'Reason
for revert:' from automated templates is misleading by suggesting the change is
reverting code rather than explaining the reland fix.

## Examples

Bad:

```cpp
// Vague reland summary with misleading 'Reason for revert:' heading:
Reland "Support thread cache indexing in allocator"

This is a reland of commit 980c24036ede221908a0aa682c948b7f42215ab7

Reason for revert: fixed

// Reland modifications omitting renamed features or trials:
Reason for reland: The intervention is changed to read asynchronously instead of skipping.
```

Good:

```cpp
// Labeled 'Reason for reland:' with specific fix and diff patchset:
Reland "Support thread cache indexing in allocator"

This is a reland of commit 980c24036ede221908a0aa682c948b7f42215ab7

Reason for reland: Fixed in patchset 2 by initializing
`opts.thread_cache_index = internal::kDefaultRootThreadCacheIndex` when
allocating test caches, resolving the bot failures on linux-chromeos-dbg.

// Detailed modifications including feature/trial renames:
Reason for reland: The intervention is changed to read asynchronously instead of
skipping, and the trial is renamed to ParallelPreReadFileMainDllWin to reflect that fact.
```
