---
name: general/eliminate-spoofable-arguments-across-privilege-boundaries
rubric: safety
severity: should_fix
---

**Rule.** When hardening an IPC or RPC endpoint to resolve identities or paths
from an authoritative local source instead of spoofable caller-supplied
arguments, eliminate the untrusted argument from all downstream logic (including
child process arguments and validation routines) rather than continuing to
forward or use it.

**Why.** Replacing an untrusted caller-supplied argument in only a subset of
operations while continuing to forward it to child processes or helper functions
leaves a privilege escalation vulnerability where spoofed inputs are still
trusted downstream.

## Examples

Bad:

```cpp
// BAD: Registry lookup uses authoritative GUID, but caller-supplied
// browser_appid is still validated and forwarded to child process.
HRESULT RunChromeRecoveryCRX(const base::FilePath& crx_path,
                             const std::wstring& browser_appid,
                             const std::wstring& browser_version, ...) {
  base::win::RegKey key(HKEY_LOCAL_MACHINE,
      install_static::GetClientsKeyPath(install_static::GetAppGuid()).c_str(),
      KEY_QUERY_VALUE);
  ValidateCRXArgs(browser_appid, browser_version, session_id);
  args.AppendSwitchNative("appguid", browser_appid);
}
```

Good:

```cpp
// GOOD: Downstream logic uses authoritative GUID; untrusted parameter is unused.
HRESULT RunChromeRecoveryCRX(const base::FilePath& crx_path,
                             const std::wstring& /*browser_appid*/,
                             const std::wstring& browser_version, ...) {
  base::win::RegKey key(HKEY_LOCAL_MACHINE,
      install_static::GetClientsKeyPath(install_static::GetAppGuid()).c_str(),
      KEY_QUERY_VALUE);
  ValidateCRXArgs(browser_version, session_id);
  args.AppendSwitchNative("appguid", install_static::GetAppGuid());
}
```
