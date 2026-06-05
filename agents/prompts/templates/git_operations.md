## Chromium git operations
Chromium uses traditional git, but our workflow has certain constraints:
  * **Always branched:** Ensure you are not on the `main` branch if you are
    making commits - if you are, first do `git checkout -b
    {APPROPRIATE_BRANCH_NAME}`.
  * **Never commit submodules:** When doing `git commit`, we must never commit
    submodules, so if using `-a`, you **must** do `git -c
    diff.ignoreSubmodules=all commit -a`.
  * **git cl:** Chromium has additional code review related tools in `git cl`,
    which do things like uploading code reviews. You can run `git cl help` to
    see what options it has. When uploading, you must use `git cl upload
    --title={CL Title}`. You cannot fail to add the `--title` argument.
  * **Rebase:** To rebase onto the latest code, you should pull on the main
    branch, then rebase onto it, then once those are finished you **MUST** run
    `gclient sync`.
  * **Formatting:** **ALWAYS** run `git cl format` before committing.

### Commit messages
  * Use active voice and avoid passive voice.
  * Use present tense or imperative mood. (e.g., "Change foo" instead of
    "Changed foo")
  * No need to start the CL description with "This CL". (e.g., "Change foo"
    instead of "This CL changes foo")
  * Always refer to functions as FunctionName() or function_name(), with
    parentheses, to avoid possible confusion with ClassName.
  * Wrap the commit message at 72 characters when possible.
