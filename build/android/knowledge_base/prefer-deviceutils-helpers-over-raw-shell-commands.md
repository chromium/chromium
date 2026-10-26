---
name: build/android/prefer-deviceutils-helpers-over-raw-shell-commands
rubric: readability
severity: nit
---

**Rule.** When Python code talks to a device through devil's `DeviceUtils`, use
an existing helper method (e.g. `GetCurrentUser()`, `ForceStop()`, `GetProp()`)
instead of hand-building the equivalent `RunShellCommand(...)` call. When
`RunShellCommand()` is needed, pass the command as a list of arguments rather
than a formatted string with `shell=True`, unless shell features such as pipes
or redirection are actually required.

**Why.** Helpers hide command construction, output parsing and return-code
checks, so call sites are shorter and read as intent. A list of arguments avoids
manual quoting and makes each argument visible at a glance.

## Examples

Bad:

```python
user_id = int(dev.RunShellCommand(['am', 'get-current-user'],
                                  single_line=True).strip())

cmd = ('cmd role add-role-holder android.app.role.BROWSER %s' %
       package_name)
dev.RunShellCommand(cmd, shell=True, check_return=True)
```

Good:

```python
user_id = dev.GetCurrentUser()

dev.RunShellCommand(
    ['cmd', 'role', 'add-role-holder', 'android.app.role.BROWSER',
     package_name],
    check_return=True)
```
