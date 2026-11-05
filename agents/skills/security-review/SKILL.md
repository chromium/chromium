---
name: security-review
description: >-
  This skill should be triggered when a security code review, or security
  variation review is requested. You will be asked to review changes the
  agent or operator has made, a diff, a bug-id, or a Gerrit CL number.
---

# Security Review

This skill guides an agent acting with the rigor and expertise of a senior
Chromium security researcher (such as the Chrome Security Team or Project Zero)
to perform security reviews of code changes and hunt for security
vulnerabilities, fix completeness issues, or unfixed variations of bugs
addressed by the operator.

The workflow evaluates code against Chromium's security threat model, process
sandboxing rules, memory safety mitigations, and IPC validation principles to
audit the target change, verify fix completeness, and trace connected code to
uncover unfixed sibling variations.

## When to Activate

Activate this skill when:

- A security code review or security variation review is requested.
- Reviewing changes the agent or operator has made, a diff, a bug ID, or a
  Gerrit CL number.
- Auditing whether identical bug patterns exist in related code for a proposed
  security fix.

Do NOT activate when:

- Implementing general non-security features or refactorings without a security
  review request.
- Conducting broad, unguided code cleanups with no security relevance.

## Subagent Execution for Unbiased Context

When running as part of a development session where coding or design interaction
with the user has already occurred, you must run this security review in a
subagent to segregate and provide an unbiased context, reusing this skill.

## Key Rules

- **Rooted in the target change:** Search for security issues and variations
  rooted in the target change and modified files. While the investigation must
  anchor on the change under review, the scope for tracing connected code
  (callers, callees, interface implementations, and data flows) as needed is not
  bounded.
- **Variation pattern matching:** For variation findings, every report must
  share the same root cause **and** bug class as the reference fix. Do not
  report unrelated bugs of the same general class (e.g., do not report an
  arbitrary use-after-free when the reference bug was a UAF caused specifically
  by a missing observer deregistration). For direct code reviews, report any
  genuine security flaw in the target code regardless of class.
- **Targeted code reading:** You read the code under question, trace execution,
  and look for related patterns. Surface-level grep matches are not enough —
  understand the relevant surrounding logic and data flow to verify the issue or
  pattern actually applies and is not mitigated by caller or callee checks.
- **Embed source file references:** Every finding must include precise
  `file:line` references in its description.
- **Distinct instances:** Explicitly distinguish each variation from the
  original fix (explain why it is a separate instance, not a duplicate).
- **Report immediately:** Report each confirmed finding as soon as it is
  verified; do not batch.
- **Subagent segregation:** When running within a development session where
  coding or design discussion has occurred, delegate the review to a subagent to
  maintain a clean, segregated, and unbiased context.
- **Read-only analysis:** Do not modify, compile, or execute code while hunting
  for vulnerabilities or variations.

## What Makes a Good Finding

- Describes a concrete security vulnerability, fix completeness issue, or
  unfixed variation with precise technical justification.
- Names the specific `file:line` where the issue exists.
- Explains the exact failure mode (e.g., missing IPC validation, unsafe
  lifetime, type confusion, integer overflow, invariant violation).
- For variations: explains how the root cause matches the reference bug or fix,
  states what distinguishes this variation from the fixed code, and cites
  `file:line` references for both locations.

## Reporting Findings

Follow the operator's request for the reporting template:

- If the request is for a **detailed** report, follow
  [`templates/detailed-security-template.md`](templates/detailed-security-template.md).
- If the request is for a **brief** report (for example inline code review
  comments), follow
  [`templates/brief-security-template.md`](templates/brief-security-template.md).
- If the request has **no preference**, follow
  [`templates/default-security-template.md`](templates/default-security-template.md).
- If the request gives a **specific different format or template**, follow that.

Template paths are relative to this skill's directory. When delegating the
review to a subagent, pass on the chosen template (its path, or the operator's
own format) along with this skill.

The templates' link bases point at `main`. Only link to `main` when the cited
line is at that line number on `main` (unchanged code, or a landed change). For
lines added or moved by an unlanded CL, link to the Gerrit patchset instead:
`https://<gerrit host>/c/<project>/+/<CL>/<patchset>/<path>#<line>`.

> [!IMPORTANT] **Note:** A brief review means the brief template, but does
> **not** mean the agent should do a less thorough security review of the code
> or variations. The template choice affects only the report format; every
> review must follow the full [Workflow](#workflow).

There is **no limit** on findings.

## Workflow

### Step 0: Resolve Input & Review Objectives

Determine the target code and review objectives from the user input:

- **Target Changes:** Identify the code under review from a local diff
  (`git diff HEAD`, literal `diff`, or empty input), a Gerrit CL
  (`crrev.com/c/\d+`, `CL:\d+`, `Change-Id: I...`), or modified file paths.
- **Bug / Vulnerability Context:** If a Bug ID (`b/\d+`, `crbug.com/\d+`) or
  written description is provided, read the bug report and analysis using
  available Buganizer tools.
- **Read Local Security Documentation:** Read `SECURITY.md` and `README.md`
  files in the directories of the changed files, and walk recursively up the
  tree to the repository root. These files capture architectural invariants,
  trust boundaries, and patterns the code is expected to respect.
- **Look for Browser-Side Validation:** Do not assume the browser blindly trusts
  renderer-supplied data; find where (and whether) it is validated.
- **Check Optional/Empty Field Handling:** Examine how the code behaves when
  fields are missing or default-initialized.
- **Determine the Review Focus:**
  - *Direct Security Review:* Auditing new code, refactorings, or changes for
    security vulnerabilities against the Chromium threat model.
  - *Fix Verification & Variation Audit:* The change fixes a vulnerability;
    verify the completeness of the operator's fix, then search for sibling
    variations.
- If a bare number is provided with no prefix and the type is ambiguous, ask the
  user whether it is a bug ID or a CL number before proceeding.

### Step 1: Direct Security Audit of the Target Change

Audit all modified and added lines in the diff/CL against the Chromium Security
Threat Model, Memory Safety Mitigations, and API Semantics:

- **IPC & Mojo Validation:** Check that renderer-supplied payloads are
  validated. Verify that `[Extensible]` enums are bounds-checked,
  `receiver_.ReportBadMessage(...)` is used on invalid data, and struct traits
  validate semantic invariants.
- **Memory Safety & Lifetimes:** Verify raw pointer usage against MiraclePtr
  exclusions (`RAW_PTR_EXCLUSION`), ensure `WeakPtr` is accessed only on its
  bound sequence, check that all Oilpan members appear in `Trace()`, and verify
  that `base::BindOnce` callbacks using `base::Unretained` have safe object
  lifetimes.
- **Concurrency & Threads:** Verify that non-thread-safe refcounted objects are
  not touched cross-thread, and ensure shared-memory buffers are copied before
  validation to prevent TOCTOU races.
- **Process Boundaries:** Verify that code running in privileged processes
  (browser, GPU on Android, network service) never implicitly trusts data from
  untrusted processes (renderers, utility processes, etc.).
- **Browser-Side Authority:** Verify that the browser relies on its own
  authoritative state rather than claims made by a child process (see
  `docs/security/mojo.md` and "Browser-Side Trust and IPC" in
  [`references/review-pitfalls.md`](references/review-pitfalls.md)).
- **Process Model, Navigation and Sandboxing:** For changes touching frames,
  navigation, document lifecycle, or process/service configuration, check the
  invariants in
  [`references/threat-model-nuances.md`](references/threat-model-nuances.md) and
  [`references/review-pitfalls.md`](references/review-pitfalls.md).

### Step 2: Verify Fix Completeness (If Reviewing a Bug Fix)

When reviewing a change that fixes a vulnerability:

- **Root Cause Resolution:** Does the change resolve the core architectural flaw
  or object lifecycle issue, rather than merely masking a symptom (such as
  adding an isolated null check or early return)?
- **Bypass & Edge Cases:** Can an attacker bypass the new check via alternate
  entry points, empty/optional fields, type confusion, or async re-entrancy?
- **Side Effects:** Does the fix introduce behavioral regressions, thread races,
  unexpected changes to object lifetime semantics, or secondary memory safety
  issues?

### Step 3: Extract the Root-Cause Pattern (For Variation Audits)

If the change under review fixes a vulnerability (or a reference bug was
provided), formalize the abstract vulnerability and fix patterns:

- What is the structural failure pattern (e.g., missing observer deregistration,
  unchecked enum cast, raw pointer held across an async task)?
- What does the vulnerable code shape look like?
- What does the complete, correct fix pattern look like?
- What test case, reproduction scenario, or execution sequence triggers the
  vulnerable condition?

### Step 4: Trace Connected Code for Sibling Variations

Search for issues rooted in the target change and modified files. Trace
connected code as needed to find where the same vulnerable pattern was missed:

- **Same Class, Sibling Methods:** Other methods on the modified classes with
  similar missing checks or state transitions.
- **Interface Implementations:** Other classes implementing the same Mojo or C++
  interface across the subsystem.
- **Callers and Callees:** Other call sites passing unchecked data to the same
  helpers, or other handlers invoking the affected APIs.
- **Connected Components:** Sibling handlers, parsers, or dispatchers in the
  same directory or connected layers.
- **Cross-Platform Implementations:** How the same feature, dialog, or API is
  implemented or hosted on every platform (Android, iOS, ChromeOS, desktop), not
  only the platform the change touches. Check `BUILDFLAG(IS_*)` branches and
  platform-specific hosts or bridges (e.g. Java/Objective-C UI layers) for the
  same missing check.

Anchor searches in the target change and modified files, tracing connected code
(callers, callees, interfaces, data-flow) as needed (see Key Rules).

### Step 5: Verify Candidates & Rate Severity

For each potential vulnerability or variation:

1. Confirm the issue is genuine: verify reachability from untrusted input or
   execution flows that trigger memory corruption or invariant violations.
   (Crossing a process or trust boundary is not strictly required;
   intra-renderer corruption or intra-component violations are valid security
   bugs).
2. Confirm it is unfixed in the current tree and not mitigated by caller or
   framework guarantees.
3. Classify severity (Critical, High, Medium, Low, Bug) using the Severity
   Rubric and `docs/security/severity-guidelines.md`.

Before moving on to Step 6, run the following verification on every finding.
Drop or rewrite any finding that does not pass; this applies regardless of which
reporting template is used.

- **Exact source line numbers:** Never cite line numbers taken from a serialized
  or structured diff representation, where wrapper syntax or interleaved deleted
  lines shift numbering relative to the real file. Compute line numbers from
  unified diff hunk headers (`@@ -old,count +new,count @@`) or verify them
  against the full file at the reviewed revision. Confirm every cited line
  physically sits inside the function named in the finding, not an adjacent one.
- **Guard verification:** For any null dereference, use-after-free, or
  dangling-pointer claim, trace every use of the pointer or member from the
  suspected invalidation point to the end of the function. Quote the exact lines
  proving that no `if (...)` guard, early return, or re-fetch already protects
  the dereference.
- **Re-entrancy boundary discipline:** When the finding involves a synchronous
  re-entrant call, nested message loop, or observer fan-out, strictly separate
  state accessed *before* the re-entrant call from state accessed *after* it.
  State dereferenced before the boundary cannot be invalidated by that call and
  must not be reported as a re-entrancy bug; only post-boundary uses are
  candidates, and those must still pass guard verification.

### Step 6: Report Findings

Report each confirmed vulnerability or variation using the template selected in
[Reporting Findings](#reporting-findings). If that template orders findings (for
example by descending severity), follow its ordering in the final report.
Whichever template is used:

- Specify the finding type (`Direct Vulnerability`, `Fix Completeness Issue`,
  `Sibling Variation`, or `Non-Security Bug`). Use `Non-Security Bug` for every
  finding rated `Bug`; the other types are only for findings with a security
  severity (Critical, High, Medium or Low).
- Cite precise `file:line` locations for both the target change and any
  connected code.
- Explicitly explain the vulnerability mechanism or how the root-cause pattern
  matches the reference fix.

## References

The canonical description of Chromium's security model, scope, and severity
lives in `docs/security`. Start with `docs/security/security-for-agents.md` (its
**Non-Bugs** list is authoritative), `docs/security/severity-guidelines.md`, and
`docs/security/mojo.md`. The skill's references only add review-specific
guidance on top of those:

- [`references/threat-model-nuances.md`](references/threat-model-nuances.md):
  Index of canonical security docs, how to treat `DCHECK` failures during
  review, scope nuances not covered by the canonical docs, and Site Isolation
  invariants for reviewers.
- [`references/review-pitfalls.md`](references/review-pitfalls.md): Code review
  heuristics complementing `docs/security/mojo.md` and
  `docs/security/origin-vs-url.md`: release vs debug assertions, memory safety
  mitigations, pointer and reference safety, browser-side trust and IPC, bounds
  checking, navigation lifecycle, and sandbox selection.

## Severity Rubric

The canonical definition of Chromium security severity levels is maintained in
`docs/security/severity-guidelines.md`. Read and consult that document for
authoritative classification rules, mitigating factors, and historical examples.
For platform sandbox distinctions, see
`docs/security/process-sandboxes-by-platform.md`.

Rate findings using the following levels:

- **Critical / High / Medium / Low:** Classify security vulnerabilities strictly
  according to `docs/security/severity-guidelines.md`. Triage based on the
  least-sandboxed platform where the issue occurs per
  `docs/security/process-sandboxes-by-platform.md` (e.g., direct-from-web memory
  corruption in the browser process or in unsandboxed GPU/network processes on
  Android is Critical; memory corruption in sandboxed processes or requiring a
  compromised renderer is High). Apply mitigating factors described in the
  guidelines (e.g., user interaction, browser shutdown, experimental flags) to
  adjust severity accordingly.
- **Bug:** Issues that are not security vulnerabilities under the Chromium
  threat model or `docs/security/severity-guidelines.md#TOC-Not-a-security-bug`
  (e.g., pure DoS / crash without memory corruption, out-of-memory resource
  exhaustion, MiraclePtr-protected UAFs, complex clickjacking, viewport-only UI
  mimics, or fullscreen exit toast quirks). Do report as `severity: Bug` to
  track functionally without cluttering security triage.

## Vulnerability Classes to Prioritize (by Security Impact)

01. Browser-process-reachable UAF / OOB write
02. GPU / network process memory corruption
03. Type confusion in privileged processes
04. Integer overflow/underflow leading to buffer overflows in privileged
    processes
05. Renderer UAF with controlled write potential
06. Renderer OOB write
07. Site isolation bypass / UXSS / Arbitrary Omnibox spoof
08. Race conditions on shared state in privileged processes
09. V8 Sandbox bypass
10. Uninitialized memory reads with attacker-observable output
