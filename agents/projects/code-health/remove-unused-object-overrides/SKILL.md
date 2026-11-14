---
name: remove-unused-object-overrides
description: Removes unused Java Object overrides (equals, hashCode, toString) from Clank UI property holders and view classes, using audit patterns and review rules so every kept override has cited usage and equals/hashCode stay paired. Use when asked to remove unused equals, hashCode, or toString overrides or to run the unused Object overrides cleanup. Don't use for other Java cleanups (use the matching code-health skill instead).
---

# Code Health: Remove Unused Object Overrides

Removes `equals()`, `hashCode()` and `toString()` overrides that no production
or test code uses, and uploads the result as a CL. It never removes an override
it can't prove unused, never removes only one of `equals()` and `hashCode()`,
and never rewrites tests to work around a removal.

## Overview

Overriding `equals()`, `hashCode()`, or `toString()` on UI property holders and
data classes adds virtual methods and metadata overhead in DEX bytecode. In many
cases, these overrides were added during initial prototyping and are never
invoked. Safely removing unreferenced overrides reduces release DEX size while
maintaining object identity semantics.

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps continuously in one
> autonomous run, without skipping steps or pausing between files or steps. Stop
> only at explicit `ask_question` gates or documented interactive stops. Use
> `rg` (ripgrep) for searches.

### Step 1: Workspace Preparation

Run the workspace preparation script:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_workspace.py --stash
```

- If the script outputs `(stashed local changes)`, inform the user.
- If the script exits non-zero, stop and report the error instead of continuing
  on a dirty or diverged tree.

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow.

### Step 3: Refactoring & Implementation

Process candidates case-by-case (one file at a time), applying modifications
cleanly. Process all files sequentially within the same run, without pausing for
user confirmation between edits, then proceed directly to Step 4:

1. **Safety Audit:**
   - Explicitly restrict scope to UI property holders and view classes.
   - Follow [Rule 2 in patterns.md](references/patterns.md) to audit all
     occurrences: check hashed collections (`HashMap`, `HashSet`),
     direct/indirect hashing (`Objects.hashCode`), collection lookups
     (`List.contains`, `List.indexOf`), state equality comparisons
     (`Objects.equals`), and serialization (Proto/Mojo/JSON).
   - **Evidence rule:** Decide each override (`equals()`, `hashCode()`,
     `toString()`, per class, including nested classes) on its own, and back
     every decision with evidence:
     - **Check blanket blocks first:** if one applies, skip the class and cite
       the annotation or category; otherwise decide each override with the keep
       and remove rules below. Classes with `@CalledByNative`, `@NativeMethods`,
       or `@JniType` in the class itself (not in another class that creates or
       passes it), domain models, and classes outside the UI property holder /
       view scope stay blocked (see review rules); citing the annotation or
       category is enough. A **domain model** is a class whose instances are
       persisted (SharedPreferences, files, a database, a `Bundle`/`Parcel`),
       cross a process boundary (Mojo, AIDL, Binder, the WebView support library
       boundary), or live under the data-layer components `find_candidates.py`
       skips (`components/autofill`, `components/payments`,
       `components/password_manager`, `components/sync`, `components/signin`;
       the same word elsewhere in a path doesn't count). Cite which criterion
       applies with the file and code that shows it; "domain model" alone is not
       a reason.
     - **Keep** an override if you can cite where that specific method is
       called, directly or implicitly (e.g. collection lookup, `PropertyModel`
       equality, string concatenation, `ArrayAdapter` rendering, a test
       assertion). Cite it as the file name plus the code that calls it, copied
       from the file (e.g. `Foo.java: mModel.set(VIEW_CONFIG, config)`), not a
       line number. The quoted code must make that method run on an instance of
       the class: either a direct call (`.equals(`, `.toString()`, `+`
       concatenation) or passing the instance to something that compares,
       hashes, or prints it (an `assertEquals` or Mockito `verify` comparing two
       different instances, `contains`, a map or set key, `PropertyModel.set`
       with a non-skip-equality key, string formatting). Code that only
       constructs it, reads its fields, or passes it to code that never
       compares, hashes, or prints it is not evidence. An annotation, a class
       category, or a usage of a different method is not evidence.
     - **Remove** an override only after running the full usage checklist for
       that method and listing each check with its result: direct callers,
       collections and lookups, `PropertyModel`/supplier equality, string
       concatenation and logs, adapters, tests, and subclasses or superclasses.
       If any check was skipped or is unclear, keep it. List the checks and
       results in the run's final summary.
     - Keep `equals()` and `hashCode()` paired (rules below).
   - Require keeping `equals()` and `hashCode()` if instances are stored as a
     key in a hashed map or set, or searched via `List.indexOf()`, `contains()`,
     or `remove()`.
   - If a superclass (other than `Object`) implements the method or a superclass
     or interface declares it, keep the override, because removal changes or
     breaks which method runs.
   - If `equals()` is required in production (e.g. Toolbar snapshot capture
     tokens) -> **Keep BOTH `equals()` and `hashCode()`.**
   - If neither `equals()` nor `hashCode()` is used -> **Remove BOTH `equals()`
     and `hashCode()`.**
   - If a unit test assertion (`assertEquals`) or Mockito verification compares
     two different instances (for example, an original and a deserialized copy),
     removal would break it -> **Keep BOTH `equals()` and `hashCode()`.** An
     assertion that compares an instance with itself still passes on identity
     and is not evidence.
2. **Apply Code Edits:**
   - Remove the unreferenced `equals()`, `hashCode()`, or `toString()` methods.
   - Clean up unused imports (such as `java.util.Objects`).
3. **Test Integrity:**
   - Treat broken unit test assertions or Mockito verification failures as
     evidence of value equality. Do not replace them with custom field assertion
     helpers or reflection matchers; keep `equals()` and `hashCode()` instead,
     because rewriting the test hides that the class needs value equality.

### Step 4: Formatting & Automated Review

1. **Format & Lint:** Run
   `python3 agents/projects/code-health/hub/scripts/format_and_lint.py` (must
   exit 0).
2. **Automated Review:** Follow
   [Automated Review](../hub/references/automated_review.md), passing
   [review_rules.md](references/review_rules.md) as
   `<SKILL_SPECIFIC_CLEANUP_RULES>`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

#### 1. Prepare Commit Message

Ensure the full `[Code Health][LSC] <Title>` subject is at most 72 characters
(use a short component name or shorten the title if needed), and generate
`../commit`:

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill remove-unused-object-overrides \
  --title "Remove unused Object overrides in <Component>" \
  --description - \
  --out-commit ../commit <<'EOF'
Remove unreferenced Java Object overrides (equals, hashCode, toString) in
<Component/Directory> to eliminate dead virtual methods from release DEX
bytecode while strictly preserving method symmetry.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill remove-unused-object-overrides \
  --validate ../commit
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-remove-object-overrides-<component-name>`
- **Commit File:** `../commit`
