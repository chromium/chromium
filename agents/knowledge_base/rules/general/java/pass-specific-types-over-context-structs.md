---
name: general/pass-specific-types-over-context-structs
rubric: design
severity: should_fix
---

**Rule.** When a function or method only operates on specific fields, tokens, or
properties from a larger context object or composite struct (such as
WorkerThreadContext or Presubmit input_api), declare the parameter as the
specific type or path directly rather than accepting the entire enclosing
context. Avoid introducing \_Impl wrapper functions whose sole purpose is to
unpack fields from a context object; design functions to accept required
parameters directly and let callers perform any necessary unpacking.

**Why.** Accepting composite context or framework objects when only specific
fields are needed creates tight coupling, introduces dead parameters, and
obscures what data the function actually relies upon. Creating trivial \_Impl
unpacking wrappers adds unnecessary indirection, whereas accepting narrow
parameters directly simplifies function signatures, eases testing, and makes
helpers reusable across different callers.

## Examples

Bad:

```cpp
// C++: Passing full context struct when only token is used
void ThreadGroupProfiler::ActiveCollection::MaybeAddWorkerThread(
    internal::WorkerThread* worker_thread,
    const WorkerThreadContext& context) {
  CreateSamplingProfilerForThread(worker_thread, context.token, sampling_params);
}

# Python: Creating an _Impl wrapper solely to unpack from context object
def _FindDeclaredFeatureParamsImpl(repository_root: pathlib.Path) -> Set[str]:
  return _SearchInFiles(_FindFeatureParamsInFile, repository_root)

def FindDeclaredFeatureParams(input_api) -> Set[str]:
  return _FindDeclaredFeatureParamsImpl(input_api.change.RepositoryRoot())
```

Good:

```cpp
// C++: Accept the specific token directly
void ThreadGroupProfiler::ActiveCollection::MaybeAddWorkerThread(
    internal::WorkerThread* worker_thread,
    const SamplingProfilerThreadToken& token) {
  CreateSamplingProfilerForThread(worker_thread, token, sampling_params);
}

# Python: Accept the specific parameter directly and let callers pass it
def FindDeclaredFeatureParams(repository_root: pathlib.Path) -> Set[str]:
  return _SearchInFiles(_FindFeatureParamsInFile, repository_root)

# Presubmit caller passes unpacked property directly:
declared_params = find_features.FindDeclaredFeatureParams(
    input_api.change.RepositoryRoot())
```
