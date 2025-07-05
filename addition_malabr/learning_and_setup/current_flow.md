## Current Flow

### Steps:

1. Your extension (JavaScript) runs in a **renderer process**.
2. It calls `chrome.yourAPI.doSomething()`.
3. Chromium sends this request over **IPC** from the renderer → browser process.
4. In the **browser process**, your C++ class (like `YourCustomFunction`) in `your_api.cc` is invoked.
5. It communicate (unix/internet socket) with the the python ml server which is running inside the podman container.