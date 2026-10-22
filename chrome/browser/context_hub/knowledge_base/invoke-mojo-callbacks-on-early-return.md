---
name: context-hub/invoke-mojo-callbacks-on-early-return
rubric: safety
severity: blocking
---

**Rule.** In `ContextHubPageHandler` and other Context Hub Mojo receivers,
always invoke the Mojo completion callback with a default or failure value on
every early-return and error path before returning.

**Why.** Dropping a Mojo response callback without running it triggers a
`DCHECK` crash in Mojo when the callback object is destroyed while the pipe is
still connected, and leaves the WebUI caller awaiting a promise that never
resolves.

## Examples

Bad:

```cpp
void ContextHubPageHandler::DeleteTopic(const std::string& topic_id,
                                        DeleteTopicCallback callback) {
  if (!context_hub_service_ || topic_id.empty()) {
    // Bug: callback is dropped without being invoked, crashing Mojo.
    return;
  }
  context_hub_service_->DeleteTopic(topic_id, std::move(callback));
}
```

Good:

```cpp
void ContextHubPageHandler::DeleteTopic(const std::string& topic_id,
                                        DeleteTopicCallback callback) {
  if (!context_hub_service_ || topic_id.empty()) {
    std::move(callback).Run(false);
    return;
  }
  context_hub_service_->DeleteTopic(topic_id, std::move(callback));
}
```
