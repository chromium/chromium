---
name: general/avoid-reusing-comptr-across-sequential-calls
rubric: readability
severity: should_fix
---

**Rule.** Declare a new, distinctly named local variable for each COM method
out-parameter result rather than reusing an existing Microsoft::WRL::ComPtr
across sequential calls. Keep each interface pointer scoped to its intended
lifetime and avoid passing the address of an active ComPtr to subsequent
queries.

**Why.** Reusing a single ComPtr variable obscures interface identities,
lifetimes, and failure states. Passing &com_ptr or IID_PPV_ARGS(&com_ptr)
invokes operator&(), which releases the existing reference before receiving the
new object; if the subsequent call fails, the prior object is already lost and
the pointer is left null.

## Examples

Bad:

```cpp
Microsoft::WRL::ComPtr<IDispatch> dispatch;
hr = shell->FindWindowSW(..., &dispatch);
if (FAILED(hr)) return hr;
// Reusing dispatch releases previous reference and obscures interface identity.
hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&dispatch));
if (FAILED(hr)) return hr;
hr = folder->get_Application(&dispatch);
```

Good:

```cpp
Microsoft::WRL::ComPtr<IDispatch> window_dispatch;
hr = shell->FindWindowSW(..., &window_dispatch);
if (FAILED(hr)) return hr;

Microsoft::WRL::ComPtr<IDispatch> background_dispatch;
hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&background_dispatch));
if (FAILED(hr)) return hr;

Microsoft::WRL::ComPtr<IDispatch> application_dispatch;
hr = folder->get_Application(&application_dispatch);
```
