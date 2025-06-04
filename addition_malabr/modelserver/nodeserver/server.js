const express = require('express');
const bodyParser = require('body-parser');
const ort = require('onnxruntime-node');
const { performance } = require('perf_hooks');

const app = express();
app.use(bodyParser.json());

let session = null;

// Endpoint to load the ONNX model.
app.post('/load_model_bert', async (req, res) => {
  try {
    session = await ort.InferenceSession.create('./mobilebert.onnx');
    res.json({ status: 'Model loaded successfully.' });
  } catch (err) {
    res.status(500).json({ error: `Failed to load model: ${err.message}` });
  }
});

// Endpoint for single inference.
app.post('/infer_single_bert', async (req, res) => {
  if (!session) {
    return res.status(400).json({ error: 'Model not loaded. Please call /load_model_bert first.' });
  }
  // For now, use a fixed dummy tokenized input.
  const inputIdsArray = [101, 7592, 1010, 2026, 3899, 2003, 1010, 102, 0, 0];
  const attentionMaskArray = [1, 1, 1, 1, 1, 1, 1, 1, 0, 0];
  try {
    const inputIdsTensor = new ort.Tensor(
      'int64',
      BigInt64Array.from(inputIdsArray.map(n => BigInt(n))),
      [1, inputIdsArray.length]
    );
    const attentionMaskTensor = new ort.Tensor(
      'int64',
      BigInt64Array.from(attentionMaskArray.map(n => BigInt(n))),
      [1, attentionMaskArray.length]
    );
    const feeds = {
      'input_ids': inputIdsTensor,
      'attention_mask': attentionMaskTensor
    };
    const output = await session.run(feeds);
    // Assume the model returns a "logits" tensor.
    res.json({ logits: output.logits.data });
  } catch (err) {
    res.status(500).json({ error: `Inference error: ${err.message}` });
  }
});

// Endpoint for batch inference.
app.post('/infer_batch_bert', async (req, res) => {
  if (!session) {
    return res.status(400).json({ error: 'Model not loaded. Please call /load_model_bert first.' });
  }
  const data = req.body;
  if (!Array.isArray(data)) {
    return res.status(400).json({ error: 'Payload must be an array.' });
  }
  const results = [];
  for (let i = 0; i < data.length; i++) {
    try {
      const inputIdsTensor = new ort.Tensor(
        'int64',
        BigInt64Array.from([101, 7592, 1010, 2026, 3899, 2003, 1010, 102, 0, 0].map(n => BigInt(n))),
        [1, 10]
      );
      const attentionMaskTensor = new ort.Tensor(
        'int64',
        BigInt64Array.from([1, 1, 1, 1, 1, 1, 1, 1, 0, 0].map(n => BigInt(n))),
        [1, 10]
      );
      const feeds = {
        'input_ids': inputIdsTensor,
        'attention_mask': attentionMaskTensor
      };
      const output = await session.run(feeds);
      results.push({ logits: output.logits.data });
    } catch (err) {
      results.push({ error: `Inference error at index ${i}: ${err.message}` });
    }
  }
  res.json(results);
});

// Dummy training endpoint.
app.post('/trainModel', async (req, res) => {
  const start = performance.now();
  await new Promise(resolve => setTimeout(resolve, 2000));
  const end = performance.now();
  res.json({ training_time_ms: (end - start), accuracy: 0.75 });
});

const PORT = 5000;
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Server is running on port ${PORT}`);
});

