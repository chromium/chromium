# Brief Security Report Template

Use this template when the operator asks for a **brief** or concise security
report, for example one suitable for inline code review comments.

This template covers only how to report. The investigation, threat model,
variation rules and severity rubric still come from the `security-review` skill.

> [!IMPORTANT] **Note:** A brief review means the brief template, but does
> **not** mean the agent should do a less thorough security review of the code
> or variations.

## Format

Report findings in **descending order of severity**, one block per finding,
using this template:

```
**Location:** [<path>:<start>-<end>](<link base><path>;l=<start>-<end>)\
**Severity:** Critical|High|Medium|Low|Bug\
**Category:** <specific bug class, e.g. Use-after-Free>

<Concisely describe what is wrong and vulnerable about the code change, why,
and what is recommended to fix the issue. Use 30 to 100 words; short and to
the point is golden. Markdown, suitable for a code review comment.>
```

## Rules

- Write the **Location:**, **Severity:** and **Category:** labels in bold, each
  on its own line (end the first two lines with two spaces so they render as
  separate lines).
- **Location** is a markdown link to the exact lines, using the repo-relative
  path in the link text. Link bases:
  - Chromium: `https://source.chromium.org/chromium/chromium/src/+/main:`
  - ChromiumOS:
    `https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:<checkout path>/`
    (e.g. `src/platform/minigbm/`)
  - Unlanded CL lines: link to the Gerrit patchset instead (see "Reporting
    Findings" in `SKILL.md`).
- Pick exactly **one** Severity, written in title case: Critical, High, Medium,
  Low or Bug.
- Pick exactly **one** Category: the most specific bug class, written in title
  case. Examples: Use-after-Free, Heap Buffer Overflow, Out-of-Bounds Read,
  Integer Overflow, Type Confusion, Uninitialized Memory, Race Condition,
  Authorization Bypass, Confused Deputy, Spoofing, Denial of Service. Use Other
  only if nothing more specific fits.
- Keep each description between **30 and 100 words**. Say what is wrong, why it
  is exploitable, and the recommended fix. Leave out background the reviewer
  already knows.
- Make only claims you verified by reading all the code involved (do not assume
  behavior in the call stack based on function or type names alone) or by an
  actual run. Never invent PoC results, CVEs, CWEs, bug IDs, dates or commit
  hashes.
- Any other `file:line` reference inside a description is also a link. Before
  delivering, re-read the report and confirm that every `file:line` reference is
  linked, and that each linked line exists in its file.
- If there are no findings, say so in one line. Optionally add one line listing
  what was checked and found correct.

## Example (fictional)

**Location:**
[example/tile_decoder.cc:47](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=47)\
**Severity:** High\
**Category:** Integer Overflow

`tiles_x * tiles_y * kTileBytes` is computed in `uint32_t` and wraps before
`Allocate()`, while the write loop at
[:88](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=88)
uses the unwrapped counts. A crafted image from any page overflows the heap in
the utility process. **Fix:** compute the size with `base::CheckMul` and reject
the image on overflow.
