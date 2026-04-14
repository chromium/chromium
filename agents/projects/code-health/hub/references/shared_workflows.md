# Shared Code Health Workflows

Use these instructions to handle generic validation and submission steps for any
Code Health cleanup task.

## Table of Contents

- [Pre-authorized Operations](#pre-authorized-operations-generic)
- [Automated Review](#automated-review)
- [Commit](#commit)
- [Upload to Gerrit](#upload-to-gerrit)

## Pre-authorized Operations (Generic)

The following operations are pre-authorized for all Code Health tasks:

- **Read-Only Discovery:** `rg`, `cs`, `ls`, `fdfind`, `glob`, `cat`, and
  `read_file`.
- **Validation & Prep:** `git cl format`, `git pull origin main --rebase`,
  `gclient sync -D`.
- **Submission:** `git cl upload -a -d`.

## Automated Review

Before finalizing any changes, a final automated review must be performed to
ensure code quality and completeness.

### Execution (Main Agent)

Delegate to the **`generalist`** sub-agent with the exact prompt below. Replace
`{Insert Git Patch Here}` with the output of `git diff HEAD`.

> You are a highly experienced code reviewer specializing in Git patches for
> Code Health and technical debt removal. Your task is to analyze the provided
> Git patch and provide comprehensive, constructive feedback.
>
> # Step by Step Instructions
>
> 1. Read the provided `patch` carefully to understand the removals and changes.
>
> 2. Analyze the `patch` for potential issues across these specific areas:
>
>    - **Functionality & Verification (CRITICAL):** Does the code still work as
>      intended? You MUST independently use tools like `cs` or `rg` to verify
>      that no orphaned references to the removed resources remain in the
>      codebase or in cross-repo dependencies. (Hint: Search for string
>      fragments or constants, not just the full name).
>    - **Security:** Are there any security vulnerabilities or unsafe
>      assumptions introduced by the cleanup?
>    - **Consistency:** Are there any inconsistencies with existing code, design
>      patterns, or XML schemas (e.g., leftover orphaned enums)?
>    - **Testing:** Does the patch include sufficient test updates to cover the
>      changes? Have tests relying on the removed code been safely updated or
>      removed?
>
> 3. Formulate concise and constructive feedback for each identified issue.
>    Start your response with a brief, user-friendly summary of the issues that
>    need to be addressed. Provide specific suggestions for remediation,
>    prioritizing critical issues over minor ones.
>
> 4. If all criteria are met and the independent verification confirms no
>    leftover references, output exactly `PASS`. Otherwise, output the complete
>    review.
>
> Patch: """ {Insert Git Patch Here} """ IMPORTANT NOTE: Start directly with the
> output, do not output any delimiters. Take a Deep Breath, read the
> instructions again, read the inputs again. Each instruction is crucial and
> must be executed with utmost care and attention to detail.

## Upload to Gerrit

**CRITICAL MANDATE:** You MUST execute these commands autonomously immediately
after committing. Do NOT ask for permission.

1. **Prep Workspace:** Run
   `git pull origin main --rebase > /dev/null 2>&1 && gclient sync -D > /dev/null 2>&1`.
2. **Upload to Gerrit:** Run `git cl upload --force --bypass-hooks -a -d`. or
   motivation for the change. Hard-wrap at 72 chars.

- **Footers:** Include `Bug: <BugID>` ONLY if `<BugID>` is a valid ID (not
  "none" or "skip"). Ensure there is a blank line before the `Bug: <BugID>`
  line.
