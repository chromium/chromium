## Chromium git operations
Chromium uses traditional git, but our workflow has certain constraints:
  * **Always branched:** Ensure you are not on the `main` branch if you are
    making commits. The `git cl` tools rely on an upstream tracking branch being
    set (e.g. set the upstream when creating via `git checkout -b
    {APPROPRIATE_BRANCH_NAME} origin/main` or after creation via `git branch
    --set-upstream-to=origin/main`).
  * **Never commit submodules:** When doing `git commit`, we must never commit
    submodules, so if using `-a`, you **must** do `git -c
    diff.ignoreSubmodules=all commit -a`.
  * **git cl:** Chromium has additional code review related tools in `git cl`,
    which do things like uploading code reviews. You can run `git cl help` to
    see what options it has.
    * **Creating a new CL:** Do **not** use `--title`. The CL title and
      description are taken directly from the commit message. Using `--title`
      on initial upload results in the CL description having the title
      duplicated.
    * **Updating an existing CL:** You **must** use `--title={Patchset Title}`
      (or `-t`) to specify the patchset description and avoid blocking on an
      interactive prompt.
    * **Updating the CL description:** The git commit is not used when uploading
      new patchsets to Gerrit; the description must be explicitly updated. Update
      an existing Gerrit change from the last git commit via
      `git cl description -n +`.
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
