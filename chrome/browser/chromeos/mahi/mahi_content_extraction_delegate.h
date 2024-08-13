// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace mahi {

struct WebContentState;

using GetContentCallback =
    base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

// This is the delegate of the mahi content extraction service. It is
// responsible for the service setup, binding and requests.
// TODO(b:336438243): updates the mojom to reflects the removal of
// CheckDistabillity().
class MahiContentExtractionDelegate {
 public:
  MahiContentExtractionDelegate();
  MahiContentExtractionDelegate(const MahiContentExtractionDelegate&) = delete;
  MahiContentExtractionDelegate& operator=(
      const MahiContentExtractionDelegate&) = delete;
  ~MahiContentExtractionDelegate();

  // Requests the content extraction service to get content from the a11y
  // update. Returns nullptr if the content cannot be extracted.
  void ExtractContent(const WebContentState& web_content_state,
                      const base::UnguessableToken& client_id,
                      GetContentCallback callback);

  // Requests the content extraction service to get content from a list of a11y
  // updates.
  void ExtractContent(const WebContentState& web_content_state,
                      const std::vector<ui::AXTreeUpdate>& updates,
                      const base::UnguessableToken& client_id,
                      GetContentCallback callback);

 private:
  // Returns true if it content extraction service is set up successfully, and
  // false otherwise.
  bool EnsureContentExtractionServiceIsSetUp();

  // Returns true if content extraction service is connected and false
  // otherwise.
  bool EnsureServiceIsConnected();

  void OnGetContentSize(const base::UnguessableToken& page_id,
                        const base::Time& start_time,
                        mojom::ContentSizeResponsePtr response);

  void OnGetContent(const base::UnguessableToken& page_id,
                    const base::UnguessableToken& client_id,
                    const GURL& url,
                    GetContentCallback callback,
                    mojom::ExtractionResponsePtr response);

  // Callback when screen ai service is initialized. If successful, binds mahi
  // content extraction service with the screen ai main content extraction
  // model.
  void OnScreenAIServiceInitialized(bool successful);

  // Binds mahi content extraction service with the Screen AI content extraction
  // service.
  void MaybeBindScreenAIContentExtraction();

  bool screen_ai_service_initialized_ = false;

  mojo::Remote<mojom::ContentExtractionServiceFactory>
      remote_content_extraction_service_factory_;
  mojo::Remote<mojom::ContentExtractionService>
      remote_content_extraction_service_;

  // This task runner is used to save the extracted content to disk. It meant to
  // be used for debugging purposes only, and should not be used in production.
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::WeakPtrFactory<MahiContentExtractionDelegate> weak_pointer_factory_{
      this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_
