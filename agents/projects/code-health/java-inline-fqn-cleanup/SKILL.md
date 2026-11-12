---
name: java-inline-fqn-cleanup
description: Identify and clean up inline fully qualified Java names (FQNs) in first-party code, replacing them with standard imports.
---

# Code Health: Java Inline FQN Cleanup

Clean up inline fully qualified names (FQNs) in Java code by replacing them with
proper import statements at the top of the file, and then formatting/optimizing
the import order.

## Overview

Using fully qualified class names inline (e.g. `android.view.View view = ...`)
instead of importing them makes code harder to read and breaks standard style
conventions. This skill helps automate the discovery, safety analysis, and
clean-up of these inline qualifiers in Chromium's first-party Java files.

**Goal:** Clean up inline fully qualified names (FQNs) in first-party Java files
and replace them with standard imports.

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
workflow, passing `--json` so each file's `valid` and `banned` FQN lists are
included and untruncated when reporting the batch.

### Step 3: Refactoring & Implementation

**CRITICAL RULE:** Standard file editing tools must be used to apply the
cleanups directly to the Java files. **DO NOT** create, write, or execute any
custom python or bash scripts to perform the text replacements.

- **References:** Refer to [patterns.md](references/patterns.md) for concrete
  examples and exceptions (e.g., handling java.lang.\* and Local R classes)
  before applying replacements.

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one FQN at a time** (rather than refactoring
all files or FQNs at once). This ensures stability and allows for precise
verification. Process all files and FQNs sequentially within the same run,
without pausing for user confirmation between edits, then proceed directly to
Step 4. For each file in the batch, apply the following cleanup rules:

1. **Avoid Name Collisions & Shadowing:** Check if the simple class name of the
   FQN is already imported, defined, or implicitly resolved in that file (e.g.,
   via wildcard imports, implicit package-private classes, or other imported
   annotations with the same name). If there is a collision or shadowing risk,
   **do not modify the line** (the FQN must remain inline).
2. **Banned FQNs:** If the discovery JSON lists any FQNs in a file's `banned`
   list, completely ignore those specific FQNs. They are either raw package
   paths or invalid classes, and attempting to import them will break the build.
3. **First-Party Code Only:** Do not modify any files in `third_party/` or
   auto-generated directories.
4. **Import Strategy for Static Members:** When cleaning up inline FQNs for
   static constants, properties (e.g. `TabProperties`), feature flags
   (`ChromeFeatureList`), or enums (`TabLaunchType`), **prefer importing the
   class itself rather than importing its members statically**.
   - *Incorrect (Static Import):*
     `import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_SELECTED;`
   - *Correct (Class Import):*
     `import org.chromium.chrome.browser.tasks.tab_management.TabProperties;`
     and use `TabProperties.IS_SELECTED` in the code.
   - *Rationale:* Qualifying static constants (e.g. `TabProperties.TITLE`) keeps
     clear context of where the constant is defined and avoids namespace
     conflicts when multiple imported properties classes share the same field
     names (e.g. `TabProperties.TITLE` vs `FolderProperties.TITLE`).
5. **Clean up Javadocs & Comments:** Replace inline FQNs found inside code
   comments and Javadoc links (e.g. `{@link android.webkit.WebSettings}` ->
   `{@link WebSettings}`). However, **never modify URLs** (like `https://...` or
   other web links) inside comments.
6. **Nested and Inner Classes:** For inline references to nested or inner
   classes (e.g. `org.chromium.chrome.browser.profiles.ProfileKey.Theme`),
   prefer importing the top-level outer class
   (`import org.chromium.chrome.browser.profiles.ProfileKey;`) and referring to
   it as `ProfileKey.Theme` in the code, rather than importing the nested class
   directly.
   - *Exception:* If importing the nested class directly is the dominant style
     in that specific file, match the existing style.
7. **Apply Replacements:**
   - Replace the inline FQN with its simple class name.
   - Add the corresponding `import` statement at the top of the file.

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
  --skill java-inline-fqn-cleanup \
  --title "Clean up inline FQNs in <Component>" \
  --description - \
  --out-commit ../commit <<'EOF'
Remove inline fully qualified class names and replace them with standard
imports in <Component/Directory>.
EOF
```

#### 2. Validate Commit File

```bash
python3 agents/projects/code-health/hub/scripts/prepare_commit_message.py \
  --skill java-inline-fqn-cleanup \
  --validate ../commit
```

If validation fails, fix the commit title or description and re-run Steps 1–2
until validation exits 0.

#### 3. Submit CL

Invoke the [Submission](../hub/references/submission.md) workflow, passing:

- **Branch Name:** `cleanup-fqns-<component-name>`
- **Commit File:** `../commit`
