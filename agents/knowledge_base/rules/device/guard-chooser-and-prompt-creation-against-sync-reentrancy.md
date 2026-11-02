---
name: device/guard-chooser-and-prompt-creation-against-sync-reentrancy
rubric: safety
severity: blocking
---

**Rule.** When calling an embedder or delegate method that creates or displays a
modal device chooser or permission prompt, capture a `base::WeakPtr` to `this`
before the call, store the returned dialog handle or closure in a stack-local
variable, and verify `if (!weak_this) return;` after the call returns before
assigning the result to a member variable.

**Why.** Displaying a chooser or permission prompt can synchronously exit
fullscreen (spinning a nested message loop that processes frame-detach IPCs) or
notify window-occlusion observers that synchronously close a Picture-in-Picture
window. Either path can destroy the hosting frame and its document-scoped
service before the chooser-creation call returns, turning a direct assignment to
a member variable (`chooser_ = delegate->RunChooser(...)`) into a use-after-free
write on freed memory.

## Examples

Bad:

```cpp
void SerialService::RequestPort(
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    const std::vector<::device::BluetoothUUID>&
        allowed_bluetooth_service_class_ids,
    RequestPortCallback callback) {
  // BAD: `RunChooser` can synchronously destroy `this` (via fullscreen drop or
  // PiP occlusion), so assigning directly to `chooser_` writes to freed memory.
  chooser_ = delegate_->RunChooser(
      &render_frame_host(), std::move(filters),
      allowed_bluetooth_service_class_ids,
      base::BindOnce(&SerialService::FinishRequestPort, base::Unretained(this),
                     std::move(callback)));
}
```

Good:

```cpp
void SerialService::RequestPort(
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    const std::vector<::device::BluetoothUUID>&
        allowed_bluetooth_service_class_ids,
    RequestPortCallback callback) {
  base::WeakPtr<SerialService> weak_this = weak_factory_.GetWeakPtr();
  std::unique_ptr<SerialChooser> chooser = delegate_->RunChooser(
      &render_frame_host(), std::move(filters),
      allowed_bluetooth_service_class_ids,
      base::BindOnce(&SerialService::FinishRequestPort,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  if (!weak_this) {
    return;
  }
  chooser_ = std::move(chooser);
}
```
