# Detailed Security Report Template

Use this template when the operator asks for a **detailed** security report (for
example a "severity validation" report or a "cause and reachability trace"). It
produces a cause-and-reachability trace for each finding, detailed enough for a
triager to confirm or challenge the severity without re-doing the investigation.

This template covers only how to report. The investigation, threat model,
variation rules and severity rubric still come from the `security-review` skill.

## Report Structure

1. **Header** (a few lines):
   - what was reviewed (commits or CLs, diff range, HEAD hash);
   - whether line numbers refer to HEAD or `main`, and any files where the two
     differ;
   - evidence basis (static reading only, or an executed PoC);
   - bug context if a bug was read (type, severity, title), and the severity
     baseline used.
2. **Findings**, in **descending order of severity**, each following the finding
   model below, separated by `---`.
3. **Direct audit:** what was checked and found correct, including changes with
   no security impact.
4. **Overall assessment:** one paragraph. Is the fix complete? What should
   happen next? What risk remains?

## Finding Model

Every finding MUST have these parts, in this order:

| Part                    | Content                                                                                                                                                                                                                                                          |
| :---------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Title line**          | `## [SEVERITY] <about 10-word title>`, where SEVERITY is CRITICAL, HIGH, MEDIUM, LOW or BUG.                                                                                                                                                                     |
| **Overview**            | One dense paragraph: which component or process; what the flaw is; how an attacker reaches it; the effect on memory or state; the trust domain; the end impact.                                                                                                  |
| **Affected files**      | Bullet list: `[path:lines](<link base><path>;l=<lines>) — role in the bug`.                                                                                                                                                                                      |
| **Summary**             | The mechanism step by step; why existing checks do not stop it; trigger inputs (labelled computed or executed); persistence or amplification; caveats that limit the impact.                                                                                     |
| **Root Cause Analysis** | One entry per link in the causal chain: **bold short claim** — `[file:lines](<link base><file>;l=<lines>)` — explanation. Add the key code as a fenced block, with a link comment on its first line and `// <--` on the faulty line. End with the **fix shape**. |

### Reachability checklist

Cover every item somewhere in the finding:

- **Input source:** who controls the triggering values, and through which API?
- **Limits:** which size, capability or permission limits apply, and which
  configurations remove them?
- **Enabling configuration:** which flags, platforms or features are needed, and
  which production configurations are in or out of scope?
- **What is created vs. what is described:** for memory bugs, the real
  allocation compared with the size or layout claimed to other code.
- **Consumer and trust domain:** who performs the bad access, and does it cross
  a trust boundary?
- **Existing checks:** each guard on the path, and why it does not help.
- **Mitigating factors:** state them plainly and adjust severity for them.

## Evidence and Honesty Rules

- Make only claims you verified by reading all the code involved (do not assume
  behavior in the call stack based on function or type names alone) or by an
  actual run. Where you could not check something, write "I did not verify X".
- Label trigger values "Computed example (not executed)" unless you actually ran
  a PoC. Show the arithmetic.
- Never invent PoC results, CVEs, CWEs, bug IDs, dates or commit hashes.
- Justify severity with the reachability trace. Name any baseline bug severity
  you rated against.
- Distinguish **Fix Completeness** gaps (same code path, not fully fixed) from
  **Sibling Variations** (separate instance, same root cause).

## Linking Rules (mandatory)

1. Every `file:line` or `file:start-end` reference is a markdown link:
   `[foo.c:10-20](<link base>foo.c;l=10-20)`.
2. In a list of ranges (`foo.c:10-20, 40-45`), link each range to the same file.
3. Bare line numbers in prose (`(239)`, `at 669`) also get links, to the file
   they refer to *in context*.
4. In fenced code, put the full URL in a comment on the first line
   (`// <url>;l=10`). Do not put markdown links inside fenced code.
5. Use the repo-relative path in link text (`dir/foo.cc:12`, not `foo.cc:12`).
6. Do not link dates, byte sizes, bit widths or example values.
7. Linked line numbers must exist at the linked revision. If HEAD is not on
   `main`, say so in the header.

| Codebase   | Link base (append `<repo-relative path>;l=<start>[-<end>]`)                                                           |
| :--------- | :-------------------------------------------------------------------------------------------------------------------- |
| Chromium   | `https://source.chromium.org/chromium/chromium/src/+/main:`                                                           |
| ChromiumOS | `https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:<checkout path>/` (e.g. `src/platform/minigbm/`) |

For lines that only exist in an unlanded CL, link to the Gerrit patchset instead
(see "Reporting Findings" in `SKILL.md`).

### Link check

Before delivering, re-read the report and confirm that every `file:line`
reference, every range in a list and every bare line number is linked, and that
each linked line exists in its file.

## Model Finding (fictional)

This example is invented, for illustration only. Copy its structure and depth,
not its content. `...` marks where a real report continues.

> ## [HIGH] Tile count overflow undersizes decode buffer in fictional codec
>
> **Overview:** In the (fictional) sandboxed image utility process,
> `ExampleTileDecoder` multiplies attacker-controlled tile counts in 32 bits,
> allocates a buffer from the wrapped product, then writes every tile into it. A
> crafted image served by any web page causes a heap overflow in the utility
> process.
>
> **Affected files:**
>
> - [example/tile_decoder.cc:40-52](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=40-52)
>   — wrapped `uint32_t` size computation
> - [example/tile_decoder.cc:88](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=88)
>   — per-tile write loop trusts the tile counts
> - ...
>
> ### Summary
>
> `tiles_x * tiles_y * kTileBytes` wraps before the call to `Allocate()`
> ([:47](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=47));
> the write loop
> ([:88](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=88))
> uses the original counts.
>
> **Computed example (not executed):** tiles 65536×65537, `kTileBytes` = 1 → the
> 32-bit product wraps to 65536 bytes allocated, while the loop writes about 4
> GiB.
>
> **Reachability:** input source: `<img>` from any page; limits: none before the
> multiply; ...
>
> ### Root Cause Analysis
>
> - **Unchecked multiply:**
>   [example/tile_decoder.cc:47](https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=47)
>   — 32-bit product used as the allocation size:
>
> ```cpp
> // https://source.chromium.org/chromium/chromium/src/+/main:example/tile_decoder.cc;l=47
> uint32_t size = tiles_x * tiles_y * kTileBytes;   // <-- wraps
> ```
>
> - ...
> - **Fix shape:** use `base::CheckMul` and reject the image on overflow.

## Quality Checklist

- [ ] Findings are in descending severity order, and each has every part of the
  finding model.
- [ ] The reachability checklist is fully covered, and mitigating factors are
  stated.
- [ ] Trigger values are labelled computed or executed, with the arithmetic
  shown.
- [ ] No invented PoC results, CVEs, CWEs, bug IDs, dates or hashes.
- [ ] Every `file:line` reference, every range in a list and every bare line
  number is linked, and every linked line exists.
- [ ] Header, direct-audit and overall-assessment sections are present.
