import asyncio
import time
from quart import Quart, request, jsonify
import torch
from transformers import pipeline, AutoTokenizer, AutoModelForQuestionAnswering, GPT2LMHeadModel, GPT2Config, GPT2Tokenizer
from datasets import load_dataset
import torch.nn.functional as F

app = Quart(__name__)

# Global variable to hold the QA pipeline.
qa_pipeline = None

@app.route('/load_model_bert', methods=['POST'])
async def load_model_bert():
    global qa_pipeline
    try:
        # Load the MobileBERT QA pipeline using the PyTorch backend.
        qa_pipeline = pipeline(
            "question-answering",
            model="csarron/mobilebert-uncased-squad-v2",
            tokenizer="csarron/mobilebert-uncased-squad-v2",
            framework="pt"
        )
        return jsonify({"status": "Model loaded successfully."})
    except Exception as e:
        return jsonify({"error": f"Failed to load model: {str(e)}"}), 500

@app.route('/infer_single_bert', methods=['POST'])
async def infer_single_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or 'question' not in data or 'context' not in data:
        return jsonify({"error": "Payload must contain 'question' and 'context' fields."}), 400

    try:
        # Offload the blocking inference call to a background thread.
        prediction = await asyncio.to_thread(qa_pipeline, {"question": data['question'], "context": data['context']})
        return jsonify(prediction)
    except Exception as e:
        return jsonify({"error": f"Inference error: {str(e)}"}), 500

@app.route('/infer_batch_bert', methods=['POST'])
async def infer_batch_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or not isinstance(data, list):
        return jsonify({"error": "Payload must be an array of objects with 'question' and 'context' fields."}), 400

    results = []
    for idx, item in enumerate(data):
        if 'question' not in item or 'context' not in item:
            results.append({"error": f"Item at index {idx} is missing 'question' or 'context'."})
        else:
            try:
                pred = await asyncio.to_thread(qa_pipeline, {"question": item['question'], "context": item['context']})
                results.append(pred)
            except Exception as e:
                results.append({"error": f"Inference error at index {idx}: {str(e)}"})
    return jsonify(results)

def sync_train_model():
    """
    This function loads a small subset of WikiText-2 and trains a small GPT-2 model using PyTorch.
    It returns the total training time and the final training loss.
    """
    # Load 1% of the WikiText-2 training data.
    dataset = load_dataset("wikitext", "wikitext-2-raw-v1", split="train[:1%]")
    texts = dataset['text']

    # Initialize GPT-2 tokenizer and set the pad token.
    tokenizer = GPT2Tokenizer.from_pretrained("gpt2")
    tokenizer.pad_token = tokenizer.eos_token

    block_size = 128  # Maximum sequence length.
    # Tokenize texts (ignoring empty lines).
    tokenized_texts = [tokenizer.encode(text) for text in texts if text.strip() != ""]
    all_tokens = []
    for tokens in tokenized_texts:
        all_tokens.extend(tokens)
    total_length = (len(all_tokens) // block_size) * block_size
    all_tokens = all_tokens[:total_length]
    # Reshape tokens into sequences.
    inputs_array = torch.tensor(all_tokens).view(-1, block_size)
    labels_array = inputs_array.clone()

    # Create a DataLoader.
    dataset_tensor = torch.utils.data.TensorDataset(inputs_array, labels_array)
    dataloader = torch.utils.data.DataLoader(dataset_tensor, batch_size=32, shuffle=True)

    # Define a small GPT-2 model configuration.
    config = GPT2Config(
        vocab_size=tokenizer.vocab_size,
        n_positions=block_size,
        n_ctx=block_size,
        n_embd=256,  # Smaller embedding dimension for speed.
        n_layer=4,   # Fewer layers.
        n_head=4
    )
    model = GPT2LMHeadModel(config)
    model.train()
    optimizer = torch.optim.Adam(model.parameters(), lr=1e-4)

    start_time = time.perf_counter()
    epochs = 3
    for epoch in range(epochs):
        for batch in dataloader:
            inputs, labels = batch
            optimizer.zero_grad()
            outputs = model(inputs, labels=labels)
            loss = outputs.loss
            loss.backward()
            optimizer.step()
    end_time = time.perf_counter()
    training_time_ms = (end_time - start_time) * 1000

    final_loss = loss.item()
    return {"training_time_ms": training_time_ms, "final_loss": final_loss}

@app.route('/train_model', methods=['POST'])
async def train_model():
    try:
        result = await asyncio.to_thread(sync_train_model)
        return jsonify(result)
    except Exception as e:
        return jsonify({"error": f"Training error: {str(e)}"}), 500

if __name__ == '__main__':
    try:
        import uvloop
        uvloop.install()
    except ImportError:
        pass
    # For local testing you can use Quart's built-in server.
    # For production, run with hypercorn:
    # hypercorn server:app --bind 0.0.0.0:5000 --worker-class uvloop --log-level debug
    app.run(host='0.0.0.0', port=5000, debug=True)

