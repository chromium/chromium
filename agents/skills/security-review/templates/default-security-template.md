# Default Security Report Template

Use this template when the operator has no preference for the report format. It
gives a medium level of detail: enough to understand, verify and fix each
finding, without the full reachability trace of the detailed template.

This template covers only how to report. The investigation, threat model,
variation rules and severity rubric still come from the `security-review` skill.

## Report Structure

1. **Header** (1-3 lines): what was reviewed (commits, CLs or diff range, and
   the HEAD hash), and whether line numbers refer to HEAD or `main`.
2. **Findings**, in **descending order of severity**, each following the finding
   model below, separated by `---`. There is **no limit** on findings.
3. **Checked and correct** (optional, a few bullets): areas of the change you
   verified and found safe.
4. **Overall assessment:** 1-3 sentences. Is the change or fix complete, and
   what should happen next?

## Finding Model

| Part                 | Content                                                                                                                                                                                                                                                                                                                                   |
| :------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Title line**       | `## [Severity] <short title>`, where Severity is Critical, High, Medium, Low or Bug.                                                                                                                                                                                                                                                      |
| **Fields**           | `**Type:**` Direct Vulnerability, Fix Completeness Issue, Sibling Variation or Non-Security Bug (use Non-Security Bug for every finding rated Bug). `**Target / Reference:**` the CL, diff or bug the finding relates to. `**Location:**` linked `file:line`. `**Category:**` specific bug class (e.g. Use-after-Free, Integer Overflow). |
| **Root Cause**       | 1 short paragraph: the security flaw or missing check, and why it is exploitable.                                                                                                                                                                                                                                                         |
| **What's Different** | Variations only: why this is a separate instance from the reference fix, with links to both locations. Omit for other types.                                                                                                                                                                                                              |
| **Details**          | 1-3 short paragraphs: how the input reaches the flaw, what goes wrong, and the main limits or mitigating factors. Include a short code excerpt only if it makes the flaw clearer.                                                                                                                                                         |
| **Fix**              | 1-2 sentences with the recommended fix.                                                                                                                                                                                                                                                                                                   |

## Evidence Rules

- Make only claims you verified by reading all the code involved (do not assume
  behavior in the call stack based on function or type names alone) or by an
  actual run.
- Label trigger values "computed example (not executed)" unless you actually ran
  a PoC.
- Never invent PoC results, CVEs, CWEs, bug IDs, dates or commit hashes.

## Linking Rules

- Every `file:line` or `file:start-end` reference is a markdown link to the
  exact lines, using the repo-relative path in the link text. This covers the
  Location field and any reference in the text.
- In fenced code, put the URL in a comment on the first line instead of a
  markdown link.
- Link bases (append `<repo-relative path>;l=<start>[-<end>]`):
  - Chromium: `https://source.chromium.org/chromium/chromium/src/+/main:`
  - ChromiumOS:
    `https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:<checkout path>/`
    (e.g. `src/platform/minigbm/`)
- For lines that only exist in an unlanded CL, link to the Gerrit patchset
  instead (see "Reporting Findings" in `SKILL.md`).

### Link check

Before delivering, re-read the report and confirm that every `file:line`
reference, every range in a list and every bare line number is linked, and that
each linked line exists in its file.

## Template

```markdown
# Security Review: <what was reviewed>

Reviewed: `<diff range>` (<CLs>), HEAD = <hash>. Line numbers refer to <HEAD|main>.

---

## [Severity] <short title>

**Type:** <Direct Vulnerability | Fix Completeness Issue | Sibling Variation | Non-Security Bug>\
**Target / Reference:** <CL / diff / bug ID>\
**Location:** [<path>:<a-b>](<link base><path>;l=<a-b>)\
**Category:** <specific bug class>

**Root Cause:** <1 short paragraph>

**What's Different:** <variations only>

**Details:** <1-3 short paragraphs, with linked references>

**Fix:** <1-2 sentences>

---

## Checked and correct
- <area> ([<path>:<a-b>](<link base><path>;l=<a-b>)): <why it is safe>

## Overall assessment
<1-3 sentences>
```

## Example Finding (fictional)

> ## [High] 32-bit tile size wraps, undersizing decode buffer
>
> **Type:** Direct Vulnerability\
> **Target / Reference:** CL 0000000 (fictional)\
> **Location:**
> [example/tile_decoder.cc:47](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=47)\
> **Category:** Integer Overflow
>
> **Root Cause:** `tiles_x * tiles_y * kTileBytes` is computed in `uint32_t` and
> wraps before `Allocate()`, but the write loop at
> [:88](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=88)
> uses the original tile counts.
>
> **Details:** Tile counts come straight from the image header, so any web page
> can supply them, and no limit is checked before the multiply. Computed example
> (not executed): 65536×65537 tiles wrap to a 65536-byte buffer while the loop
> writes about 4 GiB, overflowing the heap in the sandboxed utility process. ...
>
> **Fix:** Compute the size with `base::CheckMul` and reject the image on
> overflow.
